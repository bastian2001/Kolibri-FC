#pragma once

class OsdOutput {
public:
	virtual void sendFrame(char *frameBuffer, u8 width, u8 height);
	virtual void setSize(u8 width, u8 height);
	inline void getSize(u8 *width, u8 *height) const {
		*width = this->width;
		*height = this->height;
	}

	virtual void disableOutput() = 0;
	virtual void enableOutput() = 0;

protected:
	u8 width = 10;
	u8 height = 10;
	bool fullyTransmitted = false;
	char *frameBuffer = (char *)malloc(100);
};
