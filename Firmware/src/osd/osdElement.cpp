#include "osdElement.h"
#include "formatFunc.h"
#include "global.h"

void OsdElement::updateOsdElementData() {
	if (rawDataPtr == nullptr || !updated) {
		return; // No data to update
	}
	if (last == nullptr) {
		last = malloc(sizeof(rawDataPtr));
	}
	memcpy(last, rawDataPtr, sizeof(last));
	if (memcmp(last, rawDataPtr, sizeof(last))) return; // No change in value

		// switch (element) {
	// case elemType::BATTERY_VOLTAGE:
	// 	snprintf(screenText, 16, "%.2f\x06", adcVoltage / 100.f); //! adcViltage is in centivolts
	// 	break;

	// default:
	// 	// screenText = "ERR";
	// 	return;
	// 	break;
	// }
	lastUpdateMillis = millis();
}

bool OsdElement::isScheduled() {
	return (lastUpdateMillis <= refreshMillis && updated); // If there is no change in value no point of updating
}

u8 OsdElement::getElementType() const {
	return element;
}

void OsdElement::setRefreshRate(u8 refreshRate) {
	refreshMillis = 1000 / refreshRate;
	// TODO I lied it dosn't cap. Implement
}

void OsdElement::drawOsdElement() {
	dpWriteString(3, 5, 0x00, screenText);
	updated = false;
}

void OsdElement::setRawDataPtr(void *ptr) {
	rawDataPtr = ptr;
}

void OsdElement::setPos(u8 row, u8 column) {
	this->row = row;
	this->column = column;
}
