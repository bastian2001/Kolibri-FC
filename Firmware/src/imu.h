#include "fixedPointInt.h"
#include "quaternion.h"
#include <Arduino.h>

extern f32 pitch, roll, yaw, rpAngle;
extern i32 headMotAtt, combinedHeading, combinedHeadMot;
extern fix32 vVel, combinedAltitude;
extern fix32 eVel, nVel;
extern Quaternion q;

void imuInit();
void updateAttitude();