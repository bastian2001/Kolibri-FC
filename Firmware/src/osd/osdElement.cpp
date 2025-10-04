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
	case ElementType::ARM_TIME: {
		u32 armedSeconds = armed ? armedTimer / 1000 : 0;

		if (armedSeconds >= 3600) {
			snprintf(screenText, 32, "\x9c%d:%02d:%02d", (armedSeconds / 3600) % 3600, (armedSeconds / 60) % 60, armedSeconds % 60); // hh:mm:ss // for the rnu
		} else {
			snprintf(screenText, 32, "\x9c%d:%02d", (armedSeconds / 60) % 60, armedSeconds % 60); // mm:ss
		}
		break;
	}
	case ElementType::GPS_LATITUDE: {
		static fix64 gpsLat = gpsLatitudeFiltered;
		if (gpsLat != 0) {
			snprintf(screenText, 32, "\x89 %3.6f", currentVal.fix64_val.getf64());
		} else {
			snprintf(screenText, 32, "\x89 N/A");
		}
		break;
	}
	case ElementType::GPS_LONGITUDE: {
		static fix64 gpsLon = gpsLongitudeFiltered;
		if (gpsLon != 0) {
			snprintf(screenText, 32, "\x98 %3.6f", currentVal.fix64_val.getf64());
		} else {
			snprintf(screenText, 32, "\x98 N/A");
		}
		break;
	}

	case ElementType::BATTERY_VOLTAGE_MIN: {
		static fix32 minVoltage = 0;
		fix32 curVoltage = (fix32)adcVoltage / 100;
		if (curVoltage < minVoltage) {
			minVoltage = curVoltage;
			snprintf(screenText, 32, "%.2fV", minVoltage.getf32());
		} else {
			minVoltage = (curVoltage / ((fix32)refHz * 2)) + ((minVoltage / ((fix32)refHz * 2)) * (((fix32)refHz * 2) - 1));
			snprintf(screenText, 32, "%.2fV", minVoltage.getf32());
		}
		break;
	}
	case ElementType::FLIGHT_MODE: {
		FlightMode fm = flightMode;
		switch (fm) {
		case FlightMode::ACRO: {
			snprintf(screenText, 32, "ACRO");
			break;
		}
		case FlightMode::ANGLE: {
			snprintf(screenText, 32, "ANGL");
			break;
		}
		case FlightMode::ALT_HOLD: {
			snprintf(screenText, 32, "AHLD");
			break;
		}
		default: {
			snprintf(screenText, 32, "UKWN"); // should never hapen, but just in case "unknown" is a thing.
			break;
		}
		}
	}
	case ElementType::LINK_QUALITY: { // atual package rate : percentage of valid packages
		u8 sr = ELRS->uplinkLinkQuality;
		u8 pr = ELRS->actualPacketRate;
		snprintf(screenText, 32, "%i:%i%", pr, sr);
		break;
	}
	case ElementType::SPEED: {
		i32 rawSpeed = gpsMotion.gSpeed;
		if (rawSpeed < 0) rawSpeed *= -1;
		f32 kph = rawSpeed * (f32)3.6;
		// f32 mph = rawSpeed * (f32)2.236936; for the uncivilized
		sniprintf(screenText, 32, "%.1fkm/h", kph);
		// sniprintf(screenText, 32, "%.1fmph", mph);
		break;
	}
	case ElementType::IMU_PITCH: {
		snprintf(screenText, 32, "P:%f", pitch.getf32()); // TODO is this angle or angular momentum?
		break;
	}
	case ElementType::IMU_ROLL: {
		snprintf(screenText, 32, "R:%f", roll.getf32());
		break;
	}
	case ElementType::IMU_YAW: {
		snprintf(screenText, 32, "Y:%f", yaw.getf32());
		break;
	}
	case ElementType::HUD_COMPASS_HEADING: {
		// TODO move to hud class or implement here
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
	refHz = refreshRate;
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
