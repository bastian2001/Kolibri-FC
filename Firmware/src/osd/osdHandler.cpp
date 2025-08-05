#include "global.h"

void OsdHandler::init() {
	osdType = OsdType::DIGITAL;

	// TODO Read settings
	addOsdElement(new OsdElement(ElementType::BATTERY_VOLTAGE));

	optimize();
}

int OsdHandler::find(ElementType elementType) {
	for (int i = 0; i < MAX_OSD_ELEMENTS; ++i) {
		if (elements[i]->getElementType() == elementType) {
			return i;
		}
	}
	return -1;
}

void OsdHandler::setDigitalSize(u8 rows, u8 cols) {
	Serial.printf("Digital Size %d %d\n", rows, cols);
	digitalOsd.setCanvasSize(cols, rows);
	digitalResponse = true;
}

void OsdHandler::loop() {
	elapsedMicros taskTimer = 0;
	// ----- State machine begin -----
	State nextState = curState;
	switch (curState) {
	case State::INIT:
		nextState = State::WAITING_FOR_OSD_CONNECTION;
		break;
	case State::WAITING_FOR_OSD_CONNECTION:
		// Wait for OSD to be ready
		if (osdTimer > OSD_TIMEOUT) nextState = State::DISABLED;

		if (digitalResponse) nextState = State::CONFIGURE_OSD;

		// TODO configure functions
		break;
	case State::CONFIGURE_OSD:
		if (this->osdType == OsdType::DIGITAL) {
			digitalOsd.init();
		}
		nextState = State::IDLE;
		break;
	case State::IDLE:
		if (lastCall > minTimeout) {
			nextState = State::CHECK_UPDATES;
			lastCall = 0;
		}
		break;
	case State::CHECK_UPDATES:
		if (true) {
			elements[it]->updateOsdElementData();
			elements[it]->pushOsdElement(); //?need to rename this stuff...
		}
		if (++it >= lastElem) {
			it = 0;
			nextState = State::DRAW;
		}
		break;
	case State::DRAW:
		if (osdType == OsdType::DIGITAL) {
			digitalOsd.draw();
		}
		nextState = State::IDLE;
		break;
	case State::DISABLED:
		//?maybe still check every now and then.
		break;
	default:
		nextState = State::DISABLED;
		break;
	}
	curState = nextState;
	// ----- State machine end -----

	// if (lastCall < minTimeout) return;

	// lastCall = millis();
	// u16 start = chunk * CHUNKSIZE;
	// u16 end = (chunk == lastChunk) ? lastElem : start + CHUNKSIZE;
	// for (int i = start; i < end; i++) {
	// 	if (elements[i] != nullptr && elements[i]->getElementType() != ElementType::UNDEFINED) {
	// 		elements[i]->updateOsdElementData();
	// 		if (elements[i]->isScheduled()) {
	// 			elements[i]->drawOsdElement();
	// 		}
	// 	}
	// }
	// chunk = (chunk == lastChunk) ? 0 : chunk + 1;

	u32 duration = taskTimer; // TODO Either replace OSD_TASK or remove it from osd.cpp
	tasks[TASK_OSD].runCounter++;
	tasks[TASK_OSD].totalDuration += duration;
	if (duration < tasks[TASK_OSD].minDuration) {
		tasks[TASK_OSD].minDuration = duration;
	}
	if (duration > tasks[TASK_OSD].maxDuration) {
		tasks[TASK_OSD].maxDuration = duration;
	}
}

void OsdHandler::addOsdElement(OsdElement *element) {
	for (int i = 0; i < MAX_OSD_ELEMENTS; ++i) {
		if (elements[i] == nullptr) {
			elements[i] = element;
			return;
		}
	}
}

void OsdHandler::optimize() {

	Serial.println((u32)elements[0], HEX);
	// qsort(elements, MAX_OSD_ELEMENTS, sizeof(OsdElement *), (__compar_fn_t)OsdElement::compareOsdElements);

	// find end
	for (u16 i = 0; i < MAX_OSD_ELEMENTS; i++) {
		if (elements[i] == nullptr) { // TODO if element is disabled endFound
			lastElem = i;
			break;
		}
	}
	// find last chunk
	// lastChunk = (lastElem + CHUNKSIZE - 1) / CHUNKSIZE;

	// find highest refresh rate
	u32 min = 0xFFFFFFFF;
	for (OsdElement *element : elements) {
		if (element->getRefreshMillis() < min) min = element->getRefreshMillis();
	}
	minTimeout = (min / 2) < MAXIMUM_TIMEOUT_MILLISECONDS ? min / 2 : MAXIMUM_TIMEOUT_MILLISECONDS;
	minTimeout = minTimeout < 1 ? 1 : minTimeout;

	// optimization complete?
}
