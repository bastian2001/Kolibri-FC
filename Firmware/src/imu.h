#include "fixedPointInt.h"
#include "quaternion.h"
#include <Arduino.h>

extern float pitch, roll, yaw, rpAngle;
extern int32_t headMotAtt, combinedHeading, combinedHeadMot;
extern fixedPointInt32 vVel, combinedAltitude;
extern fixedPointInt32 eVel, nVel;
extern Quaternion q;

void imuInit();
void updateAttitude();