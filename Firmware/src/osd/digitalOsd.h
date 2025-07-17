#include "global.h"
#include "osd/osdClass.h"
#include <typedefs.h>
class DigitalOsd : public Osd {
public:
	void init() {
		u32 i = 0;
		for (u32 serialFn : serialFunctions) {
			if (serialFn == SERIAL_MSP_DISPLAYPORT) {
				MspDpSerial = i;
				break;
			}
			i++;
		}
	}
	void write() override {
		// TODO write to Digital OSD
		char *data = new char[30];
		u8 row = 0;
		u8 column = 0;
		u32 ready = (u32)serials[MspDpSerial]->availableForWrite();
		if (ready > 33) ready = 33;
		if (ready <= 3) return;
		ready -= 3; //
		frameBuffer->nextBytes(ready, data, &row, &column);
		dpWriteString(row, column, 0x00, data); // 0x00 = no blink (done by FC)
	}

	u8 getCanvasWidth() override {
		return canvasWidth;
	}
	u8 getCanvasHeight() override {
		return canvasHeight;
	}

private:
	u32 MspDpSerial = 0xFFFFFFFF;
	u8 canvasWidth = 60;
	u8 canvasHeight = 30;
};
