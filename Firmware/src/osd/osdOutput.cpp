#include "global.h"

void OsdOutput::sendFrame(char *frameBuffer, u8 width, u8 height) {
}

void OsdOutput::setSize(u8 width, u8 height) {
	void *fb = malloc(width * height);
	if (fb == nullptr) return;
	free(frameBuffer);
	frameBuffer = (char *)fb;
	this->width = width;
	this->height = height;
}
