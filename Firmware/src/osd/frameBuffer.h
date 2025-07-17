#include <typedefs.h>

class FrameBuffer {
public:
	void appendBuffer(); // called by OSD manager to add elements
	void prepareWrite(); // prepare todoFrame to be sent out
	void write(); // send todoFrame then copy to lastFrame
	bool isNew();
	void nextBytes(u8 numOfBytes, char *byteData);

private:
	// TODO OSD class -> DigitalOSD | AnalogOSD
	// void optimize(); // Optimize serial data sent via MSP_DP
	void getDiff(u32 index); // get diffrence number <index>
	u32 diffCount(); // count diffrences
	char *todoFrame[60][30];
	char *lastFrame[60][30];
};
