#include "osd/osdClass.h"
#include <typedefs.h>

class DigitalOsd : public Osd {
public:
	void write() override {
		// TODO write to Digital OSD
	}

	u8 getCanvasWidth() override {}
	u8 getCanvasHeight() override {}
};
