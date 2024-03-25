#include "fixedPointInt.h"
#include "quaternion.h"
#include <Arduino.h>

extern f32 pitch, roll, yaw;
extern i32 headMotAtt, combinedHeading, combinedHeadMot;
extern fix32 vVel, combinedAltitude;
extern fix32 eVel, nVel;
extern Quaternion q;
extern PT1 accelDataFiltered[3];
extern fix32 vAccel;

void imuInit();
void updateAttitude();