#include "osdHandler.h"

void OsdHandler::init() {
	// TODO Read settings
	for (int i = 0; i < MAX_OSD_ELEMENTS; ++i) {
		elements[i] = nullptr;
	}
	// check OSD type
	// if digital
	// if analog
	// else no OSD
}

int OsdHandler::find(u8 elementType) {
	for (int i = 0; i < MAX_OSD_ELEMENTS; ++i) {
		if (elements[i]->getElementType() == elementType) {
			return i;
		}
	}
	return -1;
}

void OsdHandler::osdHandlerLoop() {
	elapsedMicros taskTimer = 0;
	tasks[TASK_OSD].runCounter++;

	for (int i = 0; i < MAX_OSD_ELEMENTS; i++) {
		if (elements[i] != nullptr && elements[i]->getElementType() != elemType::UNDEFINED) {
			elements[i]->updateOsdElementData();
			if (elements[i]->isScheduled()) {
				elements[i]->drawOsdElement();
			}
		}
	}

	u32 duration = taskTimer; // TODO Either replace OSD_TASK or remove it from osd.cpp
	tasks[TASK_OSD].totalDuration += duration;
	if (duration < tasks[TASK_OSD].minDuration) {
		tasks[TASK_OSD].minDuration = duration;
	}
	if (duration > tasks[TASK_OSD].maxDuration) {
		tasks[TASK_OSD].maxDuration = duration;
	}
}
