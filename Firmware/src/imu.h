#include "fixedPointInt.h"
#include <Arduino.h>

extern fixedPointInt32 gyroSign[3];
extern fixedPointInt32 accelSign[3];
extern fixedPointInt32 ACCEL_UPDATE_EPS;

void imuInit();
void imuUpdateFromGyro();
void imuUpdateFromAccel();
void imuGetXyTilt(fixedPointInt32 &pitch, fixedPointInt32 &roll);