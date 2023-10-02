#pragma once
#include "fixedPointInt.h"
#include <Arduino.h>

extern const fixedPointInt32 FAST_PI;
extern const fixedPointInt32 FAST_PI_2;

extern bool debugQ;

fixedPointInt32 fastCos(fixedPointInt32 x);
fixedPointInt32 fastSin(fixedPointInt32 x);

fixedPointInt32 vectorDot(fixedPointInt32 v1[3], fixedPointInt32 v2[3]);
void            vectorCross(fixedPointInt32 v1[3], fixedPointInt32 v2[3], fixedPointInt32 out[3]);

class Quaternion {
private:
public:
    fixedPointInt32 w    = 1;
    fixedPointInt32 v[3] = {0, 0, 0};
    Quaternion(fixedPointInt32 w = 1, fixedPointInt32 x = 0, fixedPointInt32 y = 0, fixedPointInt32 z = 0);
    Quaternion(fixedPointInt32 w, fixedPointInt32 v[3]);
    Quaternion(Quaternion &q);
    Quaternion     *set(fixedPointInt32 w, fixedPointInt32 x, fixedPointInt32 y, fixedPointInt32 z);
    Quaternion     *set(fixedPointInt32 w, fixedPointInt32 v[3]);
    Quaternion     *setIdentity();
    Quaternion     *fromAxisAngle(fixedPointInt32 axis[3], fixedPointInt32 angle);
    fixedPointInt32 toAxisAngle(fixedPointInt32 axis[3]);
    Quaternion     *fromXRotation(fixedPointInt32 angle);
    Quaternion     *fromYRotation(fixedPointInt32 angle);
    Quaternion     *fromZRotation(fixedPointInt32 angle);
    fixedPointInt32 norm();
    Quaternion     *normalize();
    Quaternion      operator*(Quaternion q);
    Quaternion      operator*=(Quaternion q);
    void            rotate(fixedPointInt32 v[3], fixedPointInt32 out[3]) const;
    Quaternion     *setFromUnitVecs(fixedPointInt32 v0[3], fixedPointInt32 v1[3]);
    Quaternion     *conjugate();
};