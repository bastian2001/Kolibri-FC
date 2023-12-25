#include "fixedPointInt.h"
#include <Arduino.h>

extern float pitch, roll, yaw;
extern int32_t headMotAtt, combinedHeading, combinedHeadMot;

void imuInit();
void updateAttitude();