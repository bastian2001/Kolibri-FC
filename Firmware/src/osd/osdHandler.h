#include "global.h"
#include "osdElement.h"
#include <typedefs.h>

#define MAX_OSD_ELEMENTS 256

class OsdHandler {
public:
	OsdHandler() = default;
	~OsdHandler() = default;
	void init();
	void osdHandlerLoop();
	// void addOsdElement(OsdElement *element); //TODO unnecessary for now (no configurator side yet)
	int find(u8 elementType);

private:
	OsdElement *elements[MAX_OSD_ELEMENTS] = {nullptr};
};
