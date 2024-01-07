#include "global.h"

elapsedMillis baroStarted = 0;
bool baroMeasuring		  = false;

int16_t baroASL		  = 0, baroATO; // above sea level, above takeoff
int16_t takeoffOffset = 0;
float baroPres		  = 0;
uint8_t baroTemp	  = 0;
bool baroDataReady	  = false;

void initBaro() {
}

void baroLoop() {
}