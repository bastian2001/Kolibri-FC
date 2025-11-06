#include "global.h"

void OsdHandler::init() {
	osdType = OsdType::DIGITAL;

	// TODO Read settings
	OsdElement *elem;
	elem = new OsdElement(ElementType::BATTERY_VOLTAGE);
	elem->setPos(0, 5);
	elem->setRefreshRate(20);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::GPS_LATITUDE);
	elem->setPos(19, 5);
	elem->setRefreshRate(20);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::GPS_LONGITUDE);
	elem->setPos(20, 5);
	elem->setRefreshRate(20);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::BATTERY_VOLTAGE_MIN);
	elem->setPos(1, 5);
	elem->setRefreshRate(5);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::ARM_TIME);
	elem->setPos(3, 5);
	elem->setRefreshRate(5);
	elem = new OsdElement(ElementType::FLIGHT_MODE);
	elem->setPos(2, 5);
	elem->setRefreshRate(5);
	addOsdElement(elem);

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
	TASK_START(TASK_OSD);

	static elapsedMillis x = 0;
	if (x > 20) {
		x = 0;
		sendMspDp(MspDpFn::HEARTBEAT);
	}

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

	TASK_END(TASK_OSD);
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

OsdHandler::ElemConfig OsdHandler::getParams(u32 inWord) {
	ElemConfig element;
	// I'm like 99% sure I don't need to cast, but just in case...
	element.refreshRate = static_cast<u8>((inWord & 0xFF000000) >> 24);
	element.type = static_cast<ElementType>((inWord & 0x00FF0000) >> 16);
	element.x = static_cast<u8>((inWord & 0x0000FF) >> 8);
	element.y = static_cast<u8>((inWord & 0x000000FF) >> 0);
	return element;
}

void OsdHandler::readConfigFromLittleFs(u32 *outArr /*, Eeprom start and end chars probably*/) {
	if (littleFsReady) return;
	File settingsFile = LittleFS.open("/osd.bin", "r");
	if (!settingsFile) {
		// file does not exist
		setDefaultConfig();
		return;
	}
	if (!settingsFile.isFile()) {
		// oopsie the file is not a file :(
		settingsFile.close();
		return;
	}
	u32 word;
	u8 tmp;
	u8 i = 0;
	u8 pos = 0;
	while (settingsFile.available()) {
		tmp = settingsFile.read();
		word |= (tmp << (8 * i));
		i++;
		if (i >= 4) {
			outArr[pos++] = word;
			i = 0;
		}
	}
	if (i != 0) {
		// Serial.println("Warning: OSD file might be corrupted.");
	}
}

void writeConfigToLittleFs(u32 *inArr) {
	if (littleFsReady) return;
	File settingsFile = LittleFS.open("/osd.bin", "w");
	if (!settingsFile) return;

	for (u32 i = 0; i < MAX_OSD_ELEMENTS; i++) {
		u32 word = inArr[i];
		if (static_cast<ElementType>(word | 0x00FF0000) == ElementType::UNDEFINED) {
			continue; // skip empty.
		}
		settingsFile.write((word >> 0) & 0xFF);
		settingsFile.write((word >> 8) & 0xFF);
		settingsFile.write((word >> 16) & 0xFF);
		settingsFile.write((word >> 24) & 0xFF);
	}
}

void OsdHandler::setDefaultConfig() {
	OsdElement *elem;
	elem = new OsdElement(ElementType::BATTERY_VOLTAGE);
	elem->setPos(0, 5);
	elem->setRefreshRate(20);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::GPS_LATITUDE);
	elem->setPos(19, 5);
	elem->setRefreshRate(20);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::GPS_LONGITUDE);
	elem->setPos(20, 5);
	elem->setRefreshRate(20);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::BATTERY_VOLTAGE_MIN);
	elem->setPos(1, 5);
	elem->setRefreshRate(5);
	addOsdElement(elem);
	elem = new OsdElement(ElementType::ARM_TIME);
	elem->setPos(3, 5);
	elem->setRefreshRate(5);
	elem = new OsdElement(ElementType::FLIGHT_MODE);
	elem->setPos(2, 5);
	elem->setRefreshRate(5);
	addOsdElement(elem);
	optimize();
}
