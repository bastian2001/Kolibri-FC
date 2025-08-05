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
	switch (element) {
	case ElementType::ALTITUDE: // Probably dosn't need cm precision.
		currentVal.fix32_val = combinedAltitude;
		u8 i = 0;
		for (; currentVal.fix32_val > 10; currentVal.fix32_val /= 10) {
			screenText[0] = '0' + (currentVal.fix32_val.geti32() % 10);
			currentVal.fix32_val /= 10;
			i++;
		}
		screenText[i] = 'm';
		break;
	case ElementType::BATTERY_CELL_VOLTAGE:
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
		break;
	case ElementType::BATTERY_VOLTAGE:
		currentVal.fix32_val = (fix32)adcVoltage / (fix32)100;
		if (currentVal.fix32_val == lastVal.fix32_val) break;
		lastVal.fix32_val = currentVal.fix32_val;
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
		break;
	case ElementType::BATTERY_CELL_COUNT:
		snprintf(screenText, sizeof(screenText), "%dS", batCells);
		break;
	case ElementType::ARM_TIME:
		//snprintf(screenText, sizeof(screenText), "%d", armTime); //TODO get arm time
		break;
	case ElementType::HUD_COMPASS_HEADING:
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
