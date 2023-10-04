#include "fixedPointInt.h"
#include <Arduino.h>

extern fixedPointInt32 pitch, roll, yaw;

void imuInit();
void updateAttitude();