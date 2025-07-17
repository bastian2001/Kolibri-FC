// TODO Rename file
#include "osd/frameBuffer.h"
#include <typedefs.h>
class Osd {
public:
	Osd(FrameBuffer *framebuffer) {
		this->frameBuffer = frameBuffer;
	};
	virtual void write();
	virtual u8 getCanvasWidth();
	virtual u8 getCanvasHeight();
	virtual u8 getMaxFps();
	FrameBuffer *frameBuffer;

private:
};
