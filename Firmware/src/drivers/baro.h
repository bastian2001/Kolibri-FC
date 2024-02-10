#include "fixedPointInt.h"
#include <Arduino.h>

extern f32 baroASL;
extern f32 baroUpVel;
extern f32 baroPres;
extern u8 baroTemp;
extern fix32 gpsBaroAlt;

void initBaro();

void readBaroLoop();

void evalBaroLoop();