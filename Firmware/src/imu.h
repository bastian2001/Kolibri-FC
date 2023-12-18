#include "fixedPointInt.h"
#include <Arduino.h>

extern float pitch, roll, yaw;

void imuInit();
void updateAttitude();