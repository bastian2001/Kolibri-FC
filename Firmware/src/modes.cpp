#include "global.h"
uint32_t armingDisableFlags = 0;
bool armed					= false;
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
				armed = true;
			} else if (ELRS->consecutiveArmedCycles == 10 && ELRS->isLinkUp) {
				makeSound(2500, 599, 70, 50);
			}
		} else if (ELRS->channels[4] < 1500) {
			armed = false;
			// just disarmed, stop logging
			if (ELRS->lastChannels[4] > 1500)
				endLogging();
		}
	} else if (ELRS->sinceLastRCMessage > 500000)
		armed = false;
	crashInfo[5] = armed;
}

// // check arming
// // arming switch and already armed, or arming switch and throttle down (and not armed on boot)
// if (pChannels[4] > 1500) {
// 	crashInfo[6] = 12;
// 	if (armed) {
// 		crashInfo[6] = 13;
// 	} else if (channels[4] < 1500 && channels[4] > 0 && pChannels[2] < 1020) {
// 		crashInfo[6] = 14;
// 		startLogging();
// 		crashInfo[6] = 15;
// 		armed		 = true;
// 	} else if (channels[4] < 1500) {
// 		crashInfo[6] = 16;
// 		Serial.println(pChannels[2]);
// 		makeSound(2500, 599, 70, 50);
// 		crashInfo[6] = 17;
// 	}
// } else {
// 	crashInfo[6] = 18;
// 	armed		 = false;
// 	if (channels[4] > 1500)
// 		// just disarmed, stop logging
// 		endLogging();
// 	crashInfo[6] = 19;
// }