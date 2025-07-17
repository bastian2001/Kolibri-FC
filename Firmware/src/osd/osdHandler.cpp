#include "global.h"

OsdHandler osdHandler;

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
	if (lastCall < minTimeout) return;
	lastCall = millis();
	elapsedMicros taskTimer = 0;
	tasks[TASK_OSD].runCounter++;
	u16 start = chunk * CHUNKSIZE;
	u16 end = (chunk == lastChunk) ? lastElem : start + CHUNKSIZE;
	for (int i = start; i < end; i++) {
		if (elements[i] != nullptr && elements[i]->getElementType() != elemType::UNDEFINED) {
			elements[i]->updateOsdElementData();
			if (elements[i]->isScheduled()) {
				elements[i]->drawOsdElement();
				elements[i]->updated = false;
			}
		}
	}
	chunk = (chunk == lastChunk) ? 0 : chunk + 1;

	u32 duration = taskTimer; // TODO Either replace OSD_TASK or remove it from osd.cpp
	tasks[TASK_OSD].totalDuration += duration;
	if (duration < tasks[TASK_OSD].minDuration) {
		tasks[TASK_OSD].minDuration = duration;
	}
	if (duration > tasks[TASK_OSD].maxDuration) {
		tasks[TASK_OSD].maxDuration = duration;
	}
}

void OsdHandler::optimize() {
	// Remove Undefined Elements
	// TODO replace undefined elements with nullptr
	// Remove Disabled Elements
	// TODO move disabled elements to the back

	//  Remove Gaps
	for (u16 k = 0; k < OSD_MAX_ELEM; k++) {
		u16 gapStart = 0;
		u16 gapEnd = 0;
		bool gapStartFound = false;
		bool gapEndFound = false;
		u16 i = 0;
		do {
			if (elements[i] == nullptr) {
				gapStart = i;
				gapStartFound = true;
			}
			i++;
		} while (!gapStartFound && i < OSD_MAX_ELEM);
		if (!gapStartFound) break; // No empty spots in array
		do {
			if (elements[i] != nullptr) {
				gapEnd = i;
				gapEndFound = true;
			}
			i++;
		} while (!gapEndFound && i < OSD_MAX_ELEM);
		if (!gapEndFound) break; // No more gaps in array. Nullptr at the end
		u16 gapLength = gapEnd - gapStart;
		for (u16 j = gapStart; j + gapLength < OSD_MAX_ELEM; j++) {
			elements[j] = elements[j + gapLength];
		}
	}
	// find end
	for (u16 i; i < MAX_OSD_ELEMENTS; i++) {
		if (elements[i] = nullptr) { // TODO if element is disabled endFound
			lastElem = i;
			break;
		}
	}
	// find last chunk
	lastChunk = (lastElem + CHUNKSIZE - 1) / CHUNKSIZE;

	// find highest refresh rate
	u32 min = 0xFFFFFFFF;
	for (OsdElement *element : elements) {
		if (element->getRefreshMillis() < min) min = element->getRefreshMillis();
	}
	minTimeout = (min / 2) < MAXIMUM_TIMEOUT_MILLISECONDS ? min / 2 : MAXIMUM_TIMEOUT_MILLISECONDS;
	minTimeout = minTimeout < 0 ? 1 : minTimeout;

	//?Sort elements by refresh timer. Split chunks. Only run low refresh rate chunks when they are likely "off cooldown"
	// optimization complete?
}
