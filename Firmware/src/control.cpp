#include "global.h"

void runAcroMode() {
	fix32 smoothChannels[4]; // smoothed RC channel values (1000ish to 2000ish)
	ELRS->getSmoothChannels(smoothChannels);
}
