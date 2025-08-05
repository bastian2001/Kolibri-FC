#include "osd/osdClass.h"
#include <typedefs.h>
class DigitalOsd : public Osd {
public:
	void init();
	void write(u8 row, u8 col, const char *str, bool blinking = false) override;
	u8 getMaxFps() override { return 12; };

	void draw() override;

private:
	u8 mspSerialId = 255;
};
extern DigitalOsd digitalOsd;
