#include "global.h"

void OsdOutput::sendFrame(char *frameBuffer, u8 width, u8 height) {
	newFrame = true;
	if (width == this->width && height == this->height) {
		memcpy(this->frameBuffer, frameBuffer, width * height);
	} else if (width == this->width) {
		u8 minHeight = MIN(height, this->height);
		memcpy(this->frameBuffer, frameBuffer, width * minHeight);
		if (height < this->height) memset(&frameBuffer[height * width], 0, (this->height - height) * width);
	} else {
		u8 minWidth = MIN(width, this->width);
		u8 minHeight = MIN(height, this->height);
		memset(this->frameBuffer, 0, this->height * this->width);
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
	newFrame = true;
}
