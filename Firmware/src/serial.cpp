#include "global.h"

u32 crcLutD5[256] = {};

char const serialFunctionNames[SERIAL_FUNCTION_COUNT][20] = {
	"CRSF",
	"MSP",
	"GPS",
	"4Way",
	"Tramp",
	"Smartaudio",
	"ESC Telem",
	"MSP Displayport",
};

std::optional<KoliSerial> serials[SERIAL_COUNT];
static SerialConfig serialConfigs[SERIAL_COUNT] = {};
static u32 serialConfigsSettings[SERIAL_COUNT - 1][6] = {};
static u8 currentSerial = 0;
static u32 freeInstructions[NUM_PIOS] = {}; // we need to copy it manually, because the variable is static
static u8 freeSms[NUM_PIOS] = {};

void stopSerials() {
	elrs.reset();
	setGpsSerial(nullptr);
	end4Way();
	setTrampSerial(nullptr);

	// destroy all the serials (end() is called in the destructor)
	for (int i = 1; i < SERIAL_COUNT; i++) {
		if (serials[i]) serials[i].reset();
	}
}

static SerialUART *getSerial(int i) {
	if (i == 0) return &Serial1;
	if (i == 1) return &Serial2;
	return nullptr;
}

static void settingsToConfigs(SerialConfig cfgs[SERIAL_COUNT - 1]) {
	for (int i = 0; i < SERIAL_COUNT - 1; i++) {
		SerialConfig &cfg = cfgs[i];
		u32 *setting = serialConfigsSettings[i];
		cfg.type = (SerialType)setting[0];
		cfg.hwParam = setting[1];
		cfg.txPin = setting[2];
		cfg.rxPin = setting[3];
		cfg.baud = setting[4];
		cfg.functions = setting[5];
	}
}

static void configsToSettings(const SerialConfig cfgs[SERIAL_COUNT - 1]) {
	for (int i = 0; i < SERIAL_COUNT - 1; i++) {
		const SerialConfig &cfg = cfgs[i];
		u32 *setting = serialConfigsSettings[i];
		setting[0] = (u32)cfg.type;
		setting[1] = cfg.hwParam;
		setting[2] = cfg.txPin;
		setting[3] = cfg.rxPin;
		setting[4] = cfg.baud;
		setting[5] = cfg.functions;
	}
}

static void setSerialDefaults() {
	for (int i = 0; i < SERIAL_COUNT - 1; i++) {
		u32 *setting = serialConfigsSettings[i];
		setting[0] = 255;
		setting[1] = 0;
		setting[2] = 255;
		setting[3] = 255;
		setting[4] = 0;
		setting[5] = 0;
	}
}

bool startSerials(SerialConfig newCfgs[SERIAL_COUNT - 1]) {
	bool success = true;
	u8 elrsSerial = 0;
	KoliSerial *gpsSerial = nullptr;
	KoliSerial *trampSerial = nullptr;

	// try new config
	for (int i = 0; i < SERIAL_COUNT - 1; i++) {
		SerialConfig &cfg = newCfgs[i];
		switch (cfg.type) {
		case SerialType::UART: {
			Serial.printf("Serial %d, Type %d, params %d %d %d\n", i + 1, (u8)cfg.type, cfg.hwParam);
			Serial.flush();
			sleep_ms(10);
			SerialUART *ser = getSerial(cfg.hwParam);
			if (ser == nullptr) {
				success = false;
				break;
			}
			serials[i + 1].emplace(ser, 2048);
		} break;
		case SerialType::PIO: {
			Serial.printf("Serial %d, Type %d, params %d %d %d %d\n", i + 1, (u8)cfg.type, cfg.hwParam & 0xF, cfg.hwParam >> 4);
			Serial.flush();
			sleep_ms(10);
			if ((cfg.hwParam & 0xF) >= NUM_PIOS || (cfg.hwParam >> 4) >= NUM_PIOS) {
				success = false;
				break;
			}
			u8 txNum = cfg.hwParam & 0xF;
			u8 rxNum = cfg.hwParam >> 4;
			PIO pioTx = pio_get_instance(txNum);
			PIO pioRx = pio_get_instance(rxNum);
			serials[i + 1].emplace(pioTx, pioRx, -1, -1, 2048);
		} break;
		case SerialType::PIO_HDX: {
			Serial.printf("Serial %d, Type %d, params %d %d %d\n", i + 1, (u8)cfg.type, cfg.hwParam);
			Serial.flush();
			sleep_ms(10);
			if (cfg.hwParam >= NUM_PIOS) {
				success = false;
				break;
			}
			PIO pio = pio_get_instance(cfg.hwParam);
			serials[i + 1].emplace(pio, -1, 2048);
		} break;
		case SerialType::DISABLED:
			Serial.printf("Serial %d, Type %d, params %d %d\n", i + 1, (u8)cfg.type);
			Serial.flush();
			sleep_ms(10);
			continue;
		default:
			success = false;
			break;
		}
		KoliSerial &serial = *serials[i + 1];
		serial.functions = cfg.functions;

		u32 rxFifo = 4;
		u32 baud = 115200;
		u16 config = SERIAL_8N1;

		if (serial.functions & SERIAL_CRSF) {
			if (rxFifo < 256) rxFifo = 256;
			baud = 420000;
			elrsSerial = i + 1;
		} else if (serial.functions & SERIAL_MSP) {
			if (rxFifo < 256) rxFifo = 256;
		} else if (serial.functions & SERIAL_GPS) {
			if (rxFifo < GPS_BUF_LEN) rxFifo = GPS_BUF_LEN;
			baud = 38400;
			gpsSerial = &serial;
		} else if (serial.functions & SERIAL_4WAY_HOST) {
			if (rxFifo < 256) rxFifo = 256;
		} else if (serial.functions & SERIAL_IRC_TRAMP) {
			if (rxFifo < 32) rxFifo = 32;
			baud = 9600;
			trampSerial = &serial;
		} else if (serial.functions & SERIAL_SMARTAUDIO) {
			config = SERIAL_8N2;
			if (rxFifo < 32) rxFifo = 32;
			baud = 4800;
		} else if (serial.functions & SERIAL_ESC_TELEM) {
			if (rxFifo < 64) rxFifo = 64;
			baud = 115200;
		} else if (serial.functions & SERIAL_MSP_DISPLAYPORT) {
			if (rxFifo < 256) rxFifo = 256;
			baud = 115200;
		}
		if (cfg.baud) baud = cfg.baud;

		serial.setPinout(cfg.txPin, cfg.rxPin);
		serial.setRxFifoSize(rxFifo);
		serial.begin(baud, config);
		if (!serial) {
			success = false;
			break;
		}
	}

	if (!success) return false;

	// tell ELRS etc. about new config

	elrs.emplace(elrsSerial);
	setGpsSerial(gpsSerial);
	setTrampSerial(trampSerial);

	// save config
	for (int i = 0; i < SERIAL_COUNT - 1; i++) {
		serialConfigs[i + 1] = newCfgs[i];
	}

	configsToSettings(&serialConfigs[1]);

	KoliSerial::sinceReset = 0;
	return true;
}

const SerialConfig &getSerialConfig(int i) {
	if (i >= SERIAL_COUNT) return serialConfigs[0];
	return serialConfigs[i];
}

static bool isInstructionFree(PIO pio, int offset) {
	static const uint16_t temp_instructions[] = {0x0000};
	static const struct pio_program temp = {
		.instructions = temp_instructions,
		.length = 1,
		.origin = -1,
	};
	if (pio_can_add_program_at_offset(pio, &temp, offset))
		return true;
	else
		return false;
}

u32 getFreeInstructions(int pioNum) { return freeInstructions[pioNum]; }
u8 getFreeSms(int pioNum) { return freeSms[pioNum]; }

void initSerial() {
	addArraySetting(SETTING_SERIAL_CONFIGS, serialConfigsSettings, &setSerialDefaults);

	for (u32 i = 0; i < 256; i++) {
		u32 crc = i;
		for (u32 j = 0; j < 8; j++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ 0xD5;
			else
				crc <<= 1;
		}
		crcLutD5[i] = crc & 0xFF;
	}

	for (int i = 0; i < NUM_PIOS; i++) {
		PIO pio = pio_get_instance(i);
		for (int j = 0; j < 32; j++) {
			if (isInstructionFree(pio, j))
				freeInstructions[i] |= 1UL << j;
		}
		for (int j = 0; j < 4; j++) {
			if (!pio_sm_is_claimed(pio, j))
				freeSms[i] |= 1 << j;
		}
	}

	serials[0].emplace(&Serial, 2048);
	serials[0]->functions = SERIAL_MSP;
	// no need to begin, already begun before at boot
	serialConfigs[0] = {
		.type = SerialType::USB,
		.functions = SERIAL_MSP,
	};

	SerialConfig cfgs[SERIAL_COUNT - 1];
	settingsToConfigs(cfgs);

	startSerials(cfgs);
}

void serialLoop() {
	TASK_START(TASK_SERIAL);

	if (++currentSerial >= SERIAL_COUNT) currentSerial = 0;
	if (!serials[currentSerial]) return;

	KoliSerial &serial = *serials[currentSerial];
	u32 &functions = serial.functions;

	if (!functions) {
		while (serial.read() != -1) { // empty RX buf
			tight_loop_contents();
		}
		TASK_END(TASK_SERIAL);
		return;
	}

	// max 16 chars per loop, stop when no char to read
	for (int i = 16; i; i--) {
		int c = serial.read();
		if (c == -1) break;

		if (functions & SERIAL_CRSF) {
			if (!elrsBuffer.isFull())
				elrsBuffer.push(c);
		}
		if (functions & SERIAL_MSP) {
			rp2040.wdt_reset();
			elapsedMicros timer = 0;
			mspHandleByte(c, currentSerial);
			taskTimerTASK_SERIAL -= timer;
		}
		if (functions & SERIAL_GPS) {
			if (!gpsBuffer.isFull())
				gpsBuffer.push(c);
		}
		if (functions & SERIAL_4WAY_HOST) {
			process4Way(c);
		}
		if (functions & SERIAL_IRC_TRAMP) {
			if (!trampRxBuffer.isFull())
				trampRxBuffer.push(c);
		}
		if (functions & SERIAL_SMARTAUDIO) {
		}
		if (functions & SERIAL_ESC_TELEM) {
		}
		if (functions & SERIAL_MSP_DISPLAYPORT) {
		}
	}
	serial.loop();
	TASK_END(TASK_SERIAL);
}
