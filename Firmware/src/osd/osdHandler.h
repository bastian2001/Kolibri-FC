#pragma once
#include "global.h"
#include "osdElement.h"
#include <typedefs.h>

#define MAX_OSD_ELEMENTS 256
#define CHUNKSIZE 8
#define MAXIMUM_TIMEOUT_MILLISECONDS 5000

class OsdHandler {
public:
	OsdHandler() = default;
	~OsdHandler() = default;
	void init();
	void osdHandlerLoop();
	void addOsdElement(OsdElement *element);
	int find(u8 elementType);
	OsdElement *elements[MAX_OSD_ELEMENTS] = {nullptr};

private:
	void optimize();
	u32 minTimeout;
	u16 lastElem;
	u8 chunk;
	u8 lastChunk;
	u32 lastCall;
};

extern OsdHandler osdHandler;
