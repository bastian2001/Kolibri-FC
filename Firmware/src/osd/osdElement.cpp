#include "osdElement.h"
#include "formatFunc.h"
#include "global.h"

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
	fix32 altitude = 0;
	fix32 cellVoltage = 0;
	fix32 batVoltage = 0;
	switch (element) {
	case ElementType::ALTITUDE: // Probably dosn't need cm precision.
		altitude = combinedAltitude;
		u8 i = 0;
		for (; altitude > 10; altitude /= 10) {
			screenText[0] = '0' + (altitude.geti32() % 10);
			altitude /= 10;
			i++;
		}
		screenText[i] = 'm';
		break;
	case ElementType::BATTERY_CELL_VOLTAGE:
		static fix32 lastCellVoltage = 0;
		cellVoltage = (fix32)adcVoltage / (fix32)batCells / (fix32)100;
		if (cellVoltage == lastCellVoltage) break;
		lastCellVoltage = cellVoltage;
		if (advOpt & OSD_ELEMENT_ADV_OPT_SHOW_CELLS) {
			snprintf(screenText, sizeof(screenText), "%fV[%d]", cellVoltage, batCells);
		} else {
			snprintf(screenText, sizeof(screenText), "%fV", cellVoltage);
		}
		if (advOpt & OSD_ELEMENT_ADV_OPT_NO_BLINK || cellVoltage > emptyVoltageSetting) {
			blinking = false;
		} else {
			blinking = true;
		}
		break;
	case ElementType::BATTERY_VOLTAGE:
		static fix32 lastBatVoltage = 0;
		batVoltage = (fix32)adcVoltage / (fix32)100;
		if (batVoltage == lastBatVoltage) break;
		lastBatVoltage = batVoltage;
		if (advOpt & OSD_ELEMENT_ADV_OPT_SHOW_CELLS) {
			snprintf(screenText, sizeof(screenText), "%fV[%d]", batVoltage, batCells);
		} else {
			snprintf(screenText, sizeof(screenText), "%fV", batVoltage);
		}
		if (advOpt & OSD_ELEMENT_ADV_OPT_NO_BLINK || batVoltage > emptyVoltageSetting) {
			blinking = false;
		} else {
			blinking = true;
		}
		break;
	case ElementType::BATTERY_CELL_COUNT:
		break;
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
	refreshRate = refreshRate <= refreshRate ? refreshRate : maxHz;
	refreshMillis = 1000 / refreshRate;
}

void OsdElement::drawOsdElement() {
	// TODO implement
}

void OsdElement::pushOsdElement() {
	dpWriteString(3, 5, 0x00, screenText);
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
