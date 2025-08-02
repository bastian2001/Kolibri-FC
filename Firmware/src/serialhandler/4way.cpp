#include "global.h"
#include "pioasm/onewire_receive.pio.h"
#include "pioasm/onewire_transmit.pio.h"

bool setup4WayDone = false;
u32 offsetPioReceive;
u32 offsetPioTransmit;
pio_sm_config configPioReceive;
pio_sm_config configPioTransmit;
u8 serialNum4Way;

bool isTxEnabled = false;
std::deque<uint8_t> escRxBuf;

#define INTERFACE_NAME_4WAY "m4wFCIntf"
#define SERIAL_4WAY_VER_MAIN 20
#define SERIAL_4WAY_VER_SUB_1 (uint8_t)0
#define SERIAL_4WAY_VER_SUB_2 (uint8_t)05

#define SERIAL_4WAY_PROTOCOL_VER 108

#define SERIAL_4WAY_VERSION (uint16_t)((SERIAL_4WAY_VER_MAIN * 1000) + (SERIAL_4WAY_VER_SUB_1 * 100) + SERIAL_4WAY_VER_SUB_2)

#define SERIAL_4WAY_VERSION_HI (uint8_t)(SERIAL_4WAY_VERSION / 100)
#define SERIAL_4WAY_VERSION_LO (uint8_t)(SERIAL_4WAY_VERSION % 100)
#define IM_ARM_BLB 4

enum class State4Way {
	IDLE, // waiting for '/'
	CMD,
	ADDR_HI,
	ADDR_LO,
	LEN,
	PAYLOAD,
	CHECKSUM_HI,
	CHECKSUM_LO
};
enum class Res4Way {
	ACK_OK = 0x00,
	NACK_INVALID_CMD = 0x02,
	NACK_INVALID_CRC = 0x03,
	NACK_VERIFY_ERROR = 0x04,
	NACK_INVALID_CHANNEL = 0x08,
	NACK_INVALID_PARAM = 0x09,
	NACK_GENERAL_ERROR = 0x0F
};
enum class BlRes {
	SUCCESS = 0x30,
	ERROR_VERIFY = 0xC0,
	ERROR_COMMAND = 0xC1,
	ERROR_CRC = 0xC2,
	NONE = 0xFF
};
enum class Cmd4Way {
	INTERFACE_TEST_ALIVE = 0x30,
	PROTOCOL_GET_VERSION = 0x31,
	INTERFACE_GET_NAME = 0x32,
	INTERFACE_GET_VERSION = 0x33,
	INTERFACE_EXIT = 0x34,
	DEVICE_RESET = 0x35,
	DEVICE_INIT_FLASH = 0x37,
	DEVICE_ERASE_ALL = 0x38,
	DEVICE_PAGE_ERASE = 0x39,
	DEVICE_READ = 0x3A,
	DEVICE_WRITE = 0x3B,
	DEVICE_C2CK_LOW = 0x3C,
	DEVICE_READ_EEPROM = 0x3D,
	DEVICE_WRITE_EEPROM = 0x3E,
	INTERFACE_SET_MODE = 0x3F,
	DEVICE_VERIFY = 0x40,
};
#define RESTART_BOOTLOADER_4WAY 0
#define EXIT_BOOTLOADER_4WAY 1
enum class BlCmd {
	RUN_CMD = 0x00,
	PROG_FLASH = 0x01,
	ERASE_FLASH = 0x02,
	READ_FLASH_SIL = 0x03,
	VERIFY_FLASH = 0x03,
	VERIFY_FLASH_ARM = 0x04,
	READ_EEPROM = 0x04,
	PROG_EEPROM = 0x05,
	READ_SRAM = 0x06,
	READ_FLASH_ATM = 0x07,
	BOOTINIT = 0x07,
	BOOTSIGN = 0x08,
	KEEP_ALIVE = 0xFD,
	SET_BUFFER = 0xFE,
	SET_ADDRESS = 0xFF,
};

void pioSetProgram(uint offset, pio_sm_config c) {
	pio_sm_set_config(PIO_ESC, 0, &c);
	pio_sm_exec(PIO_ESC, 0, pio_encode_jmp(offset));
}

void pioResetESC() {
	pioSetProgram(offsetPioTransmit, configPioTransmit);
	sleep_ms(1);
	pio_sm_exec_wait_blocking(PIO_ESC, 0, pio_encode_set(pio_pins, 0));
	elapsedMillis x = 0;
	while (x < 300)
		rp2040.wdt_reset();
	pio_sm_exec_wait_blocking(PIO_ESC, 0, pio_encode_set(pio_pins, 1));
	pioSetProgram(offsetPioReceive, configPioReceive);
}

void changePin(uint8_t newPin) {
	sm_config_set_in_pins(&configPioReceive, newPin);
	sm_config_set_set_pins(&configPioReceive, newPin, 1);
	sm_config_set_jmp_pin(&configPioReceive, newPin);
	sm_config_set_out_pins(&configPioTransmit, newPin, 1);
	sm_config_set_set_pins(&configPioTransmit, newPin, 1);
	if (isTxEnabled) {
		pioSetProgram(offsetPioTransmit, configPioTransmit);
	} else {
		pioSetProgram(offsetPioReceive, configPioReceive);
	}
}

void pioEnableTx() {
	pioSetProgram(offsetPioTransmit, configPioTransmit);
	isTxEnabled = true;
}
void pioDisableTx() {
	while (!pio_sm_is_tx_fifo_empty(PIO_ESC, 0)) {
	}
	while (pio_sm_get_pc(PIO_ESC, 0) != offsetPioTransmit + 2) {
	}
	pioSetProgram(offsetPioReceive, configPioReceive);
	isTxEnabled = false;
}

void pioWrite(uint8_t data) {
	pio_sm_put_blocking(PIO_ESC, 0, data);
}

uint32_t pioAvailable() {
	return escRxBuf.size();
}
uint8_t pioRead() {
	uint8_t data = escRxBuf.front();
	escRxBuf.pop_front();
	return data;
}

void delayWhileRead(uint16_t ms) {
	elapsedMillis x = 0;
	do {
		if (pio_sm_get_rx_fifo_level(PIO_ESC, 0)) {
			escRxBuf.push_back(pio_sm_get(PIO_ESC, 0) >> 24);
		}
		rp2040.wdt_reset();
	} while (x < ms);
}
void delayMicrosWhileRead(uint16_t us) {
	elapsedMicros x = 0;
	do {
		if (pio_sm_get_rx_fifo_level(PIO_ESC, 0)) {
			escRxBuf.push_back(pio_sm_get(PIO_ESC, 0) >> 24);
		}
		rp2040.wdt_reset();
	} while (x < us);
}

uint16_t byteCrc4Way(uint8_t data, uint16_t crc) {
	uint8_t xb = data;
	for (uint8_t i = 0; i < 8; i++) {
		if (((xb & 0x01) ^ (crc & 0x0001)) != 0) {
			crc = crc >> 1;
			crc = crc ^ 0xA001;
		} else {
			crc = crc >> 1;
		}
		xb = xb >> 1;
	}
	return crc;
}

u16 crcUpdateXmodem(u16 crc, u8 c) {
	crc ^= (u16)c << 8;
	for (u8 i = 0; i < 8; i++) {
		if (crc & 0x8000)
			crc = (crc << 1) ^ 0x1021;
		else
			crc <<= 1;
	}
	return crc;
}

void sendEsc(uint8_t tx_buf[], uint16_t buf_size, bool CRC = true) {
	uint16_t i = 0;
	u16 esc_crc = 0;
	if (buf_size == 0) {
		buf_size = 256;
	}
	pioEnableTx();
	for (i = 0; i < buf_size; i++) {
		pioWrite(tx_buf[i]);
		esc_crc = byteCrc4Way(tx_buf[i], esc_crc);
	}
	if (CRC) {
		pioWrite(esc_crc & 0xff);
		pioWrite((esc_crc >> 8) & 0xff);
		buf_size = buf_size + 2;
	}
	pioDisableTx();
}

uint16_t getEsc(uint8_t rx_buf[], uint16_t wait_ms) {
	uint16_t i = 0;
	bool timeout = false;
	while ((!pioAvailable()) && (!timeout)) {
		delayWhileRead(1);
		i++;
		if (i >= wait_ms) {
			timeout = true;
			return 0;
		}
	}
	i = 0;
	while (pioAvailable()) {
		rx_buf[i] = pioRead();
		i++;
		delayMicrosWhileRead(500);
	}
	return i;
}

void begin4Way(u8 serialNum) {
	if (setup4WayDone) return;
	deinitESCs();
	for (int i = 0; i < 4; i++) {
		pio_gpio_init(PIO_ESC, PIN_MOTORS + i);
	}
	offsetPioReceive = pio_add_program(PIO_ESC, &onewire_receive_program);
	offsetPioTransmit = pio_add_program(PIO_ESC, &onewire_transmit_program);
	pio_sm_claim(PIO_ESC, 0);
	configPioReceive = onewire_receive_program_get_default_config(offsetPioReceive);
	sm_config_set_set_pins(&configPioReceive, PIN_MOTORS, 1);
	sm_config_set_in_pins(&configPioReceive, PIN_MOTORS);
	sm_config_set_jmp_pin(&configPioReceive, PIN_MOTORS);
	sm_config_set_in_shift(&configPioReceive, true, false, 32);
	sm_config_set_clkdiv_int_frac(&configPioReceive, 859, 128);
	configPioTransmit = onewire_transmit_program_get_default_config(offsetPioTransmit);
	sm_config_set_set_pins(&configPioTransmit, PIN_MOTORS, 1);
	sm_config_set_out_pins(&configPioTransmit, PIN_MOTORS, 1);
	sm_config_set_out_shift(&configPioTransmit, true, false, 8);
	sm_config_set_clkdiv_int_frac(&configPioTransmit, 859, 128);
	pio_sm_init(PIO_ESC, 0, offsetPioReceive, &configPioReceive);
	pio_sm_set_enabled(PIO_ESC, 0, true);
	serials[serialNum].functions |= SERIAL_4WAY;
	serialNum4Way = serialNum;
	setup4WayDone = true;
}

void end4Way() {
	if (!setup4WayDone) return;
	pio_sm_set_enabled(PIO_ESC, 0, false);
	pio_sm_unclaim(PIO_ESC, 0);
	pio_remove_program(PIO_ESC, &onewire_receive_program, offsetPioReceive);
	pio_remove_program(PIO_ESC, &onewire_transmit_program, offsetPioTransmit);
	for (i32 i = 0; i < 4; i++) {
		gpio_set_function(PIN_MOTORS + i, GPIO_FUNC_NULL);
	}
	initESCs();
	serials[serialNum4Way].functions &= ~SERIAL_4WAY;
	setup4WayDone = false;
}

void send4WayResponse(u8 cmd, u16 address, u8 *payload = nullptr, u16 len = 1, Res4Way resCode = Res4Way::ACK_OK) {
	u8 dummy = 0;
	if (payload == nullptr && len != 1)
		return;
	else if (payload == nullptr && len == 1)
		payload = &dummy;
	if (len == 0) len = 256;
	u16 crc = 0;
	u8 header[5] = {0x2E, cmd, (u8)(address >> 8), (u8)(address & 0xFF), (u8)(len & 0xFF)};
	for (u16 i = 0; i < 5; i++) {
		crc = crcUpdateXmodem(crc, header[i]);
	}
	for (u16 i = 0; i < len; i++) {
		crc = crcUpdateXmodem(crc, payload[i]);
	}
	crc = crcUpdateXmodem(crc, (u8)resCode);
	Serial.write(header, 5);
	Serial.write(payload, len);
	Serial.write((u8)resCode);
	Serial.write(crc >> 8);
	Serial.write(crc & 0xFF);
	Serial.flush();
}

uint8_t blSendCmdSetAddr(uint8_t addrHi, uint8_t addrLo) {
	if (addrHi == 0xFF && addrLo == 0xFF) return 1;
	uint8_t sCmd[] = {(u8)BlCmd::SET_ADDRESS, 0x00, addrHi, addrLo};
	uint8_t rxBuf[50] = {0};
	sendEsc(sCmd, 4);
	delayWhileRead(5);
	uint16_t rxSize = getEsc(rxBuf, 20);
	// return (rxSize ? rxBuf[rxSize - 1] : brNONE) == brSUCCESS;
	return rxSize && rxBuf[rxSize - 1] == (u8)BlRes::SUCCESS;
}

uint8_t blSendCmdSetBuf(uint8_t len, uint8_t buf[256]) {
	uint8_t sCmd[] = {(u8)BlCmd::SET_BUFFER, 0x00, len == 0, len};
	uint8_t rxBuf[50] = {0};
	sendEsc(sCmd, 4);
	delayWhileRead(5);
	uint16_t rxSize = getEsc(rxBuf, 20);
	if (rxSize && rxBuf[rxSize - 1] != (u8)BlRes::NONE) return 0;

	sendEsc(buf, len);
	delayWhileRead(5);
	rxSize = getEsc(rxBuf, 80);
	return rxSize && rxBuf[rxSize - 1] == (u8)BlRes::SUCCESS;
}

uint8_t blVerifyFlash(uint8_t len, uint8_t buf[256], uint8_t addrHi, uint8_t addrLo) {
	if (blSendCmdSetAddr(addrHi, addrLo)) {
		uint8_t sCmd[] = {(u8)BlCmd::VERIFY_FLASH_ARM, 0x01};
		uint8_t rxBuf[50] = {0};
		if (!blSendCmdSetBuf(len, buf)) return 0;
		sendEsc(sCmd, 2);
		delayWhileRead(5);
		uint16_t rxSize = getEsc(rxBuf, 20);
		return rxSize ? rxBuf[rxSize - 1] : (u8)BlRes::NONE;
	}
	return 0;
}

void process4WayCmd(u8 cmd, u16 address, u8 *payload, u16 len) {
	u8 buf[300] = {0};

	switch ((Cmd4Way)cmd) {
	case Cmd4Way::INTERFACE_TEST_ALIVE:
		buf[0] = (u8)BlCmd::KEEP_ALIVE;
		buf[1] = 0;
		sendEsc(buf, 2);
		delayWhileRead(5);
		getEsc(buf, 200); // data is ignored
		send4WayResponse(cmd, address);
		break;

	case Cmd4Way::PROTOCOL_GET_VERSION:
		buf[0] = 108; // version number
		send4WayResponse(cmd, address, buf);
		break;

	case Cmd4Way::INTERFACE_GET_NAME:
		send4WayResponse(cmd, address, (u8 *)INTERFACE_NAME_4WAY, strlen(INTERFACE_NAME_4WAY));
		break;

	case Cmd4Way::INTERFACE_GET_VERSION:
		buf[0] = SERIAL_4WAY_VERSION_HI;
		buf[1] = SERIAL_4WAY_VERSION_LO;
		send4WayResponse(cmd, address, buf, 2);
		break;

	case Cmd4Way::INTERFACE_EXIT:
		end4Way();
		send4WayResponse(cmd, address);
		break;

	case Cmd4Way::DEVICE_RESET:
		if (payload[0] < 4) { // ESC count
			changePin(PIN_MOTORS + payload[0]);
			buf[0] = RESTART_BOOTLOADER_4WAY;
			buf[1] = 0;
			sendEsc(buf, 2);
			pioResetESC();
			getEsc(buf, 50); // data is ignored
			send4WayResponse(cmd, address);
		} else {
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_INVALID_CHANNEL);
		}
		break;

	case Cmd4Way::DEVICE_INIT_FLASH:
		if (payload[0] < 4) { // ESC count
			changePin(PIN_MOTORS + payload[0]);
			u8 bootInit[] = {0, 0, 0, 0, 0, 0, 0, 0, 0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
			sendEsc(bootInit, 17, false);
			delayWhileRead(50);
			u8 rxSize = getEsc(buf, 200);
			if (rxSize && buf[rxSize - 1] == (u8)BlRes::SUCCESS) {
				buf[0] = buf[5]; // Device Signature2?
				buf[1] = buf[4]; // Device Signature1?
				buf[2] = buf[3]; // "c"?
				buf[3] = IM_ARM_BLB;
				send4WayResponse(cmd, address, buf, 4);
			} else {
				buf[0] = 0x06; // Device Signature2?
				buf[1] = 0x33; // Device Signature1?
				buf[2] = 0x67; // "c"?
				buf[3] = IM_ARM_BLB; // Boot Pages?
				send4WayResponse(cmd, address, buf, 4, Res4Way::NACK_GENERAL_ERROR);
			}
		} else {
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_INVALID_CHANNEL);
		}
		break;

	case Cmd4Way::DEVICE_READ: {
		buf[0] = (u8)BlCmd::SET_ADDRESS;
		buf[1] = 0;
		buf[2] = address >> 8;
		buf[3] = address & 0xFF;
		sendEsc(buf, 4);
		delayWhileRead(5);
		u16 rxSize = getEsc(buf, 200);
		if (buf[0] == (u8)BlRes::SUCCESS) {
			buf[0] = (u8)BlCmd::READ_FLASH_SIL;
			buf[1] = payload[0];
			sendEsc(buf, 2);
			if (payload[0])
				delayWhileRead(payload[0]);
			else
				delayWhileRead(256);
			rxSize = getEsc(buf, 500);
			if (rxSize) {
				u16 rxCrc = 0;
				if (buf[rxSize - 1] != (u8)BlRes::SUCCESS) {
					send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_GENERAL_ERROR);
					break;
				}
				rxSize -= 3;
				for (i16 i = 0; i < rxSize; i++) {
					rxCrc = byteCrc4Way(buf[i], rxCrc);
				}
				rxCrc = byteCrc4Way(buf[rxSize], rxCrc);
				rxCrc = byteCrc4Way(buf[rxSize + 1], rxCrc);
				if (rxCrc) {
					send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_GENERAL_ERROR);
				} else {
					send4WayResponse(cmd, address, buf, rxSize);
				}
			} else {
				send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_GENERAL_ERROR);
			}
		} else {
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_VERIFY_ERROR);
		}
	} break;

	case Cmd4Way::DEVICE_WRITE: {
		buf[0] = (u8)BlCmd::SET_ADDRESS;
		buf[1] = 0;
		buf[2] = address >> 8;
		buf[3] = address & 0xFF;
		sendEsc(buf, 4);
		delayWhileRead(50);
		u16 rxSize = getEsc(buf, 100);
		if (buf[0] != (u8)BlRes::SUCCESS) {
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_GENERAL_ERROR);
			break;
		}
		buf[0] = (u8)BlCmd::SET_BUFFER;
		buf[1] = 0;
		buf[2] = len >> 8;
		buf[3] = len & 0xFF;
		sendEsc(buf, 4);
		delayWhileRead(5);
		sendEsc(payload, len);
		delayWhileRead(5);
		rxSize = getEsc(buf, 200);
		if (buf[0] != (u8)BlRes::SUCCESS) {
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_GENERAL_ERROR);
			break;
		}
		buf[0] = (u8)BlCmd::PROG_FLASH;
		buf[1] = 1;
		sendEsc(buf, 2);
		delayWhileRead(30);
		rxSize = getEsc(buf, 100);
		if (buf[0] == (u8)BlRes::SUCCESS)
			send4WayResponse(cmd, address);
		else
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_GENERAL_ERROR);
	} break;

	case Cmd4Way::INTERFACE_SET_MODE:
		if (payload[0] == 4)
			send4WayResponse(cmd, address);
		else
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_INVALID_PARAM);
		break;

	case Cmd4Way::DEVICE_VERIFY:
		switch ((BlRes)blVerifyFlash(len, payload, address >> 8, address & 0xFF)) {
		case BlRes::SUCCESS:
			send4WayResponse(cmd, address);
			break;
		case BlRes::ERROR_VERIFY:
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_VERIFY_ERROR);
			break;
		default:
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_GENERAL_ERROR);
			break;
		}
		break;

	case Cmd4Way::DEVICE_PAGE_ERASE: {
		u8 ack = (u8)Res4Way::ACK_OK;
		u8 rx[250] = {0};

		buf[0] = (u8)BlCmd::SET_ADDRESS;
		buf[1] = 0;
		buf[2] = (payload[0]) << 2;
		buf[3] = 0;
		sendEsc(buf, 4);
		delayWhileRead(5);
		getEsc(rx, 200);
		if (rx[0] != (u8)BlRes::SUCCESS)
			ack = (u8)Res4Way::NACK_GENERAL_ERROR;

		buf[0] = (u8)BlCmd::ERASE_FLASH;
		buf[1] = 0x01;
		sendEsc(buf, 2);
		delayWhileRead(50);
		getEsc(rx, 100);
		if (rx[0] != (u8)BlRes::SUCCESS)
			ack = (u8)Res4Way::NACK_GENERAL_ERROR;

		send4WayResponse(cmd, address, nullptr, 1, (Res4Way)ack);
	} break;

	default:
		send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_INVALID_CMD);
		break;
	}
}

void process4Way(u8 c) {
	if (!setup4WayDone) return;
	static u8 cmd;
	static u16 address;
	static u16 len;
	static u16 crc;
	static u16 crcIn;
	static u8 payload[256];
	static u16 pos;
	static State4Way state = State4Way::IDLE;
	switch (state) {
	case State4Way::IDLE:
		if (c == '/') {
			pos = 0;
			crc = crcUpdateXmodem(0, c);
			state = State4Way::CMD;
		}
		break;
	case State4Way::CMD:
		cmd = c;
		crc = crcUpdateXmodem(crc, c);
		state = State4Way::ADDR_HI;
		break;
	case State4Way::ADDR_HI:
		address = c << 8;
		crc = crcUpdateXmodem(crc, c);
		state = State4Way::ADDR_LO;
		break;
	case State4Way::ADDR_LO:
		address |= c;
		crc = crcUpdateXmodem(crc, c);
		state = State4Way::LEN;
		break;
	case State4Way::LEN:
		len = c;
		if (!len) len = 256;
		crc = crcUpdateXmodem(crc, c);
		state = State4Way::PAYLOAD;
		break;
	case State4Way::PAYLOAD:
		payload[pos++] = c;
		crc = crcUpdateXmodem(crc, c);
		if (pos == len) {
			state = State4Way::CHECKSUM_HI;
		}
		break;
	case State4Way::CHECKSUM_HI:
		crcIn = (u16)c << 8;
		state = State4Way::CHECKSUM_LO;
		break;
	case State4Way::CHECKSUM_LO:
		crcIn |= c;
		if (crc == crcIn) {
			process4WayCmd(cmd, address, payload, len);
		} else {
			send4WayResponse(cmd, address, nullptr, 1, Res4Way::NACK_INVALID_CRC);
		}
		state = State4Way::IDLE;
		break;
	}
}
