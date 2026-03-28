#pragma once
#include "osdOutput.h"

class AnalogOsdOutput : public OsdOutput {
public:
	AnalogOsdOutput(const AnalogOsdOutput &) = delete;
	AnalogOsdOutput &operator=(const AnalogOsdOutput &) = delete;
	[[gnu::const]] static AnalogOsdOutput &get() {
		static AnalogOsdOutput a;
		return a;
	}

	void begin();
	void loop();

	void updateCharacter(u8 cmAddr, u8 data[54]);

	void setSize(u8 width, u8 height) override;

private:
	AnalogOsdOutput() {
		setSize(30, 16);
	}

	void disableOutput();
	void enableOutput();

	u8 isReady = 0;
	elapsedMicros checkTimer = 0;
	u8 drawingLine = 0;

	enum {
		REG_VM0,
		REG_VM1,
		REG_HOS,
		REG_VOS,
		REG_DMM,
		REG_DMAH,
		REG_DMAL,
		REG_DMDI,
		REG_CMM,
		REG_CMAH,
		REG_CMAL,
		REG_CMDI,
		REG_OSDM,
		REG_RB0 = 0x10,
		REG_OSDBL = 0x6C,
		REG_STAT = 0xA0,
		REG_DMDO = 0xB0,
		REG_CMDO = 0xC0,
	};
};
