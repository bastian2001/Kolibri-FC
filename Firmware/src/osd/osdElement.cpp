#include "osdElement.h"
#include "formatFunc.h"
#include "global.h"

int OsdElement::compareOsdElements(const OsdElement *a, const OsdElement *b) {
	if (a == nullptr && b == nullptr) return 0;
	if (a == nullptr) return 1;
	if (b == nullptr) return -1;

	if (a->getElementType() == ElementType::UNDEFINED && a->getElementType() == ElementType::UNDEFINED) return 0;
	if (a->getElementType() == ElementType::UNDEFINED) return 1;
	if (b->getElementType() == ElementType::UNDEFINED) return -1;

	if (a->getRefreshMillis() == b->getRefreshMillis()) return 0;
	if (a->getRefreshMillis() > b->getRefreshMillis()) return 1;
	if (a->getRefreshMillis() < b->getRefreshMillis()) return -1;
	return 0;
}

void OsdElement::updateOsdElementData() {
	//? <note_001> Implement further if data is no longer global
	// if (rawDataPtr == nullptr || !updated) {
	// 	return; // No data to update
	// }
	// if (last == nullptr) {
	// 	last = malloc(sizeof(rawDataPtr));
	// }
	// memcpy(last, rawDataPtr, sizeof(last));
	// if (memcmp(last, rawDataPtr, sizeof(last))) return; // No change in value
	switch (element) {
	case ElementType::ALTITUDE: { // Probably dosn't need cm precision.
		currentVal.fix32_val = combinedAltitude;
		u8 i = 0;
		for (; currentVal.fix32_val > 10; currentVal.fix32_val /= 10) {
			screenText[0] = '0' + (currentVal.fix32_val.geti32() % 10);
			currentVal.fix32_val /= 10;
			i++;
		}
		screenText[i] = 'm';
	} break;
	case ElementType::BATTERY_CELL_VOLTAGE: {
		static fix32 lastCellVoltage = 0;
		currentVal.fix32_val = (fix32)adcVoltage / (fix32)batCells / (fix32)100;
		if (currentVal.fix32_val == lastCellVoltage) break;
		lastCellVoltage = currentVal.fix32_val;
		if (advOpt & OSD_ELEMENT_ADV_OPT_SHOW_CELLS) {
			snprintf(screenText, sizeof(screenText), "%fV[%d]", currentVal.fix32_val, batCells);
		} else {
			snprintf(screenText, sizeof(screenText), "%fV", currentVal.fix32_val);
		}
		if (advOpt & OSD_ELEMENT_ADV_OPT_NO_BLINK || currentVal.fix32_val > emptyVoltageSetting) {
			blinking = false;
		} else {
			blinking = true;
		}
	} break;
	case ElementType::BATTERY_VOLTAGE: {
		fix32 &voltage = currentVal.fix32_val;
		voltage = (fix32)adcVoltage / 100;
		if (voltage == lastVal.fix32_val) break;
		if (advOpt & OSD_ELEMENT_ADV_OPT_SHOW_CELLS) {
			snprintf(screenText, sizeof(screenText), "%fV[%d]", voltage.getf32(), batCells);
		} else {
			snprintf(screenText, 32, "%.2fV", voltage.getf32());
		}
		if (advOpt & OSD_ELEMENT_ADV_OPT_NO_BLINK || voltage > batCells * emptyVoltageSetting) {
			blinking = false;
		} else {
			blinking = true;
		}
		lastVal.fix32_val = voltage;
	} break;
	case ElementType::BATTERY_CELL_COUNT:
		snprintf(screenText, sizeof(screenText), "%dS", batCells);
		break;
	case ElementType::ARM_TIME:
		// snprintf(screenText, sizeof(screenText), "%d", armTime); //TODO get arm time
		break;
	case ElementType::HUD_COMPASS_HEADING: {
		currentVal.fix32_val = combinedHeading;
		//|----S----|----W----|----N----|----E----|----S----|
		if (currentVal.fix32_val == lastVal.fix32_val) break;
		lastVal.fix32_val = currentVal.fix32_val;
		u8 heading = (u8)((currentVal.fix32_val.getf32()) * RAD_TO_DEG);
		screenText[0] = '0' + (heading % 10);
		heading /= 10;
		screenText[1] = '0' + (heading % 10);
		heading /= 10;
		screenText[2] = '0' + (heading % 10);
		heading /= 10;
		screenText[3] = '0' + (heading % 10);
		screenText[4] = ' ';
		screenText[5] = 'S';
		screenText[6] = ' ';
		screenText[7] = 'W';
		screenText[8] = ' ';
		screenText[9] = 'N';
		screenText[10] = ' ';
		screenText[11] = 'E';
		screenText[12] = ' ';
		screenText[13] = 'S';
	} break;
	}
	updated = false; //? still neded?
	lastUpdateMillis = millis();
}

bool OsdElement::isScheduled() {
	return (lastUpdateMillis <= refreshMillis && updated); // If there is no change in value no point of updating
}

ElementType OsdElement::getElementType() const {
	return element;
}

void OsdElement::setRefreshRate(u8 refreshRate) {
	if (refreshRate > maxHz) {
		refreshRate = maxHz;
	}
	refreshMillis = 1000 / refreshRate;
}

void OsdElement::drawOsdElement() {
	// TODO implement
}

void OsdElement::pushOsdElement() {
	digitalOsd.write(row, column, screenText, blinking);
	updated = false;
}

//? <note_001> Not needed for now.
// void OsdElement::setRawDataPtr(void *ptr) {
// 	rawDataPtr = ptr;
// }

void OsdElement::setPos(u8 row, u8 column) {
	this->row = row;
	this->column = column;
}
