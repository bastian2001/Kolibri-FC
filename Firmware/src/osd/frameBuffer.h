#include <typedefs.h>

class frameBuffer {
public:
	void appendBuffer(); // called by OSD manager to add elements
	void prepareWrite(); // prepare todoFrame to be sent out
	void write(); // send todoFrame then copy to lastFrame

private:
	// TODO OSD class -> DigitalOSD | AnalogOSD
	void optimize(); // Optimize serial data sent via MSP_DP
	void getDiff(u32 index); // get diffrence number <index>
	u32 diffCount(); // count diffrences
	char *todoFrame[60][30];
	char *lastFrame[60][30];
};
