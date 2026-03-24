#include "global.h"

void OsdOutput::sendFrame(char *frameBuffer, u8 width, u8 height) {
	fullyTransmitted = false;
	if (width == this->width) {
		u8 minHeight = MIN(height, this->height);
		memcpy(this->frameBuffer, frameBuffer, width * minHeight);
	} else {
		u8 minWidth = MIN(width, this->width);
		u8 minHeight = MIN(height, this->height);
		for (int row = 0; row < minHeight; row++) {
			memcpy(&this->frameBuffer[row * this->width], &frameBuffer[row * width], minWidth);
		}
	}
}

void OsdOutput::setSize(u8 width, u8 height) {
	void *fb = malloc(width * height);
	if (fb == nullptr) return;
	free(frameBuffer);
	frameBuffer = (char *)fb;
	this->width = width;
	this->height = height;
	fullyTransmitted = false;
}
