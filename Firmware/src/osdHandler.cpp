#include "osdHandler.h"

void OsdHandler::init() {
	// TODO Read settings
	for (int i = 0; i < MAX_OSD_ELEMENTS; ++i) {
		elements[i] = nullptr;
	}
}

bool OsdHandler::exist(u8 elementType) {
	for (int i = 0; i < MAX_OSD_ELEMENTS; ++i) {
		if (elements[i]->getElementType() == elementType) {
			return true;
		}
	}
	return false;
}
