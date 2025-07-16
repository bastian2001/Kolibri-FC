#include "osdElement.h"
#include "global.h"

void OsdElement::updateOsdElementData() {
	if (rawDataPtr == nullptr) {
		return; // No data to update
	}

	switch (element) {
	case elemType::BATTERY_VOLTAGE:
		snprintf(screenText, 16, "%.2f\x06", adcVoltage / 100.f); //! adcViltage is in centivolts
		break;

	default:
		// screenText = "ERR";
		return;
		break;
	}
	lastUpdateMillis = millis();
}

u8 OsdElement::getElementType() const {
	return element;
}
