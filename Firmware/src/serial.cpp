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
	if (elrs) elrs->setupSubscription(0, nullptr, 0, nullptr, MspVersion::V2);
	elrs.reset();
	setGpsSerial(nullptr);
	end4Way();
	setTrampSerial(nullptr);
	bbStopPrinting();
	lastMspSerial = nullptr;

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

void revertSerials() {
	stopSerials();
	startSerials(&serialConfigs[1]);
}

bool startSerials(SerialConfig newCfgs[SERIAL_COUNT - 1]) {
	bool success = true;
	KoliSerial *elrsSerial = 0;
	KoliSerial *gpsSerial = nullptr;
	KoliSerial *trampSerial = nullptr;

	// try new config
	for (int i = 0; i < SERIAL_COUNT - 1; i++) {
		SerialConfig &cfg = newCfgs[i];
		switch (cfg.type) {
		case SerialType::UART: {
			SerialUART *ser = getSerial(cfg.hwParam);
			if (ser == nullptr) {
				success = false;
				break;
			}
			serials[i + 1].emplace(ser, 2048);
		} break;
		case SerialType::PIO: {
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
			if (cfg.hwParam >= NUM_PIOS) {
				success = false;
				break;
			}
			PIO pio = pio_get_instance(cfg.hwParam);
			serials[i + 1].emplace(pio, -1, 2048);
		} break;
		case SerialType::DISABLED:
			continue;
		default:
			success = false;
			break;
		}
		KoliSerial &serial = *serials[i + 1];
		serial.setFunctions(cfg.functions);

		u32 rxFifo = 4;
		u32 baud = 115200;
		u16 config = SERIAL_8N1;

		if (cfg.functions & SERIAL_CRSF) {
			if (rxFifo < 256) rxFifo = 256;
			baud = 420000;
			elrsSerial = &serial;
		} else if (cfg.functions & SERIAL_MSP) {
			if (rxFifo < 256) rxFifo = 256;
		} else if (cfg.functions & SERIAL_GPS) {
			if (rxFifo < GPS_BUF_LEN) rxFifo = GPS_BUF_LEN;
			baud = 38400;
			gpsSerial = &serial;
		} else if (cfg.functions & SERIAL_4WAY_HOST) {
			if (rxFifo < 256) rxFifo = 256;
		} else if (cfg.functions & SERIAL_IRC_TRAMP) {
			if (rxFifo < 32) rxFifo = 32;
			baud = 9600;
			trampSerial = &serial;
		} else if (cfg.functions & SERIAL_SMARTAUDIO) {
			config = SERIAL_8N2;
			if (rxFifo < 32) rxFifo = 32;
			baud = 4800;
		} else if (cfg.functions & SERIAL_ESC_TELEM) {
			if (rxFifo < 64) rxFifo = 64;
			baud = 115200;
		} else if (cfg.functions & SERIAL_MSP_DISPLAYPORT) {
			if (rxFifo < 256) rxFifo = 256;
			// baud = MSP_DP_SPEED;
		}
		if (cfg.baud) baud = cfg.baud;

		if (!pinIsAllowed(cfg.txPin) || !pinIsAllowed(cfg.rxPin)) {
			success = false;
			break;
		}

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
	serials[0]->setFunctions(SERIAL_MSP);
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
	const u32 &functions = serial.functions();

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
		if (functions & (SERIAL_MSP | SERIAL_MSP_DISPLAYPORT)) {
			rp2040.wdt_reset();
			elapsedMicros timer = 0;
			serial.mspParser().handleByte(c);
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
	}
	serial.loop();
	TASK_END(TASK_SERIAL);
}
