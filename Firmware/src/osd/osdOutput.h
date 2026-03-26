#pragma once

class OsdOutput {
public:
	OsdOutput() {};
	OsdOutput(const OsdOutput &) = delete;
	OsdOutput &operator=(const OsdOutput &) = delete;
	virtual ~OsdOutput() {
		if (frameBuffer != nullptr) free(frameBuffer);
	}

	virtual void sendFrame(char *frameBuffer, u8 width, u8 height);
	virtual void setSize(u8 width, u8 height);
	inline void getSize(u8 *width, u8 *height) const {
		*width = this->width;
		*height = this->height;
	}
	virtual void loop() = 0;

	virtual void disableOutput() = 0;
	virtual void enableOutput() = 0;

protected:
	u8 width = 30;
	u8 height = 16;
	bool fullyTransmitted = false;
	char *frameBuffer = (char *)malloc(30 * 16);
};
