#pragma once

class OsdOutput {
public:
	virtual void sendFrame(char *frameBuffer, u8 width, u8 height);
	void setSize(u8 width, u8 height);

private:
	char *frameBuffer = (char *)malloc(100);
	u8 width = 10;
	u8 height = 10;
};
