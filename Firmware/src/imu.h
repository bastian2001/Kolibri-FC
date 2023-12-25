#include "fixedPointInt.h"
#include <Arduino.h>

extern float pitch, roll, yaw;
extern int32_t headMotAtt, combinedHeading;

void imuInit();
void updateAttitude();