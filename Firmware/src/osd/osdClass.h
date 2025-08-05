#pragma once
#include "osd/frameBuffer.h"
#include <typedefs.h>
class Osd {
public:
	Osd() = default;

	virtual void write(u8 row, u8 col, const char *str, bool blinking = false);
	virtual u8 getMaxFps();
	// FrameBuffer *frameBuffer;

	u8 getCanvasWidth() {
		return canvasWidth;
	}
	u8 getCanvasHeight() {
		return canvasHeight;
	}
	void setCanvasSize(u8 width, u8 height) {
		canvasWidth = width;
		canvasHeight = height;
	}

	virtual void draw();

private:
	u8 canvasWidth = 60;
	u8 canvasHeight = 30;
};
