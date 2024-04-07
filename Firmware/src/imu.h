#include "fixedPointInt.h"
#include "quaternion.h"
#include <Arduino.h>

extern f32 pitch, roll, yaw;
extern fix32 combinedHeading;
extern PT1 magHeadingCorrection;
extern fix32 vVel, combinedAltitude;
extern fix32 eVel, nVel;
extern Quaternion q;
extern PT1 accelDataFiltered[3];
extern fix32 vAccel;

void imuInit();
void updateAttitude();