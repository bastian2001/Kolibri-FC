#include "global.h"
uint32_t armingDisableFlags = 0;
bool armed					= false;
FLIGHT_MODE lastFlightMode	= FLIGHT_MODE::LENGTH;
int32_t startPointLat, startPointLon;
// 0: switch in armed position for >= 10 cycles
// 1: throttle down
// 2: GPS fix and >= 6 satellites
// 3: configurator not attached
// 4: at least one good ELRS packet before arming
// 5: allowed modes during arm: acro, angle
// 6: Gyro calibrated

void modesLoop() {
	if (ELRS->newPacketFlag & 0x00000001) {
		ELRS->newPacketFlag &= 0xFFFFFFFE;
		if (!armed) {
			if (ELRS->consecutiveArmedCycles >= 10)
				armingDisableFlags &= 0xFFFFFFFE;
			else
				armingDisableFlags |= 0x00000001;
			if (ELRS->channels[2] < 1020)
				armingDisableFlags &= 0xFFFFFFFD;
			else
				armingDisableFlags |= 0x00000002;
			// if (lastPvtMessage > 1000)
			// 	armingDisableFlags |= 0x00000004;
			if (configuratorConnected && configOverrideMotors > 1000)
				armingDisableFlags |= 0x00000008;
			else
				armingDisableFlags &= 0xFFFFFFF7;
			if (ELRS->isLinkUp)
				armingDisableFlags &= 0xFFFFFFEF;
			else
				armingDisableFlags |= 0x00000010;
			if (flightMode <= FLIGHT_MODE::ANGLE)
				armingDisableFlags &= 0xFFFFFFDF;
			else
				armingDisableFlags |= 0x00000020;
			if (!armingDisableFlags) {
				startLogging();
				armed		  = true;
				startPointLat = gpsMotion.lat;
				startPointLon = gpsMotion.lon;
			} else if (ELRS->consecutiveArmedCycles == 10 && ELRS->isLinkUp) {
				makeSound(2500, 599, 70, 50);
			}
		} else if (ELRS->channels[4] < 1500) {
			armed = false;
			// just disarmed, stop logging
			if (ELRS->lastChannels[4] > 1500)
				endLogging();
		}
		flightMode = (FLIGHT_MODE)((ELRS->channels[6] - 900) / 200);
		if (flightMode >= FLIGHT_MODE::LENGTH)
			flightMode = FLIGHT_MODE::ACRO;
		if (flightMode != lastFlightMode) {
			switch (flightMode) {
			case FLIGHT_MODE::ACRO:
				updateElem(OSDElem::FLIGHT_MODE, "ACRO ");
				break;
			case FLIGHT_MODE::ANGLE:
				updateElem(OSDElem::FLIGHT_MODE, "ANGLE");
				break;
			case FLIGHT_MODE::ALT_HOLD:
				updateElem(OSDElem::FLIGHT_MODE, "ALT  ");
				break;
			case FLIGHT_MODE::GPS_VEL:
				updateElem(OSDElem::FLIGHT_MODE, "GPSV ");
				break;
			case FLIGHT_MODE::GPS_POS:
				updateElem(OSDElem::FLIGHT_MODE, "GPSP ");
				break;
			default:
				break;
			}
			lastFlightMode = flightMode;
		}
	} else if (ELRS->sinceLastRCMessage > 500000)
		armed = false;
	crashInfo[5] = armed;
}

void modesInit(){
	placeElem(OSDElem::FLIGHT_MODE, 1, 2);
	enableElem(OSDElem::FLIGHT_MODE);
}