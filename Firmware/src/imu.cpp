#include "global.h"

const fixedPointInt32 FAST_PI            = PI;
const fixedPointInt32 FAST_PI_2          = PI / 2;
const fixedPointInt32 RAW_TO_RAD_PER_SEC = PI * 4000 / 180; // 2000deg per second, but raw is only +/-.5
const fixedPointInt32 RAW_TO_M_PER_SEC2  = 9.81 * 32;       // same reason, +/-16g
const fixedPointInt32 FRAME_TIME         = fixedPointInt32(1) / fixedPointInt32(3200);
const fixedPointInt32 FACULTY_FOUR       = 1. / 24.;
const fixedPointInt32 FACULTY_SIX        = 1. / 720.;
const fixedPointInt32 FAST_ACOS_A        = -0.939115566365855;
const fixedPointInt32 FAST_ACOS_B        = 0.9217841528914573;
const fixedPointInt32 FAST_ACOS_C        = -1.2845906244690837;
const fixedPointInt32 FAST_ACOS_D        = 0.295624144969963174;
const fixedPointInt32 QUARTER            = 0.25;
const fixedPointInt32 ALPHA_997          = 0.997;
const fixedPointInt32 SQRT_A             = 1.51658;
const fixedPointInt32 SQRT_B             = 0.32969;
const fixedPointInt32 SQRT_C             = 0.0358359;
const fixedPointInt32 SQRT_D             = 0.00779041;
const fixedPointInt32 TWO_POINT_THREE    = 2.3;
const fixedPointInt32 INVALID_LOW_THRES  = fixedPointInt32(9) / RAW_TO_M_PER_SEC2;
const fixedPointInt32 INVALID_HIGH_THRES = fixedPointInt32(11) / RAW_TO_M_PER_SEC2;

// const fixedPointInt32 FILTER_ALPHA           = fixedPointInt32(.997);
// const fixedPointInt32 FILTER_ONE_MINUS_ALPHA = fixedPointInt32(1) - FILTER_ALPHA;

fixedPointInt32 pitch, roll, yaw;

void imuInit() {
    pitch = 0; // pitch forward
    roll  = 0; // roll right
    yaw   = 0; // yaw right
}

fixedPointInt32 map(fixedPointInt32 x, fixedPointInt32 in_min, fixedPointInt32 in_max, fixedPointInt32 out_min, fixedPointInt32 out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

fixedPointInt32 fastAtan(fixedPointInt32 x) {
    // https://www.dsprelated.com/showarticle/1052.php
    if (x < 0) {
        return -fastAtan(-x);
    }
    if (x > 1) {
        return FAST_PI_2 - fastAtan(fixedPointInt32(1) / x);
    }
    return x / (fixedPointInt32(1) + fixedPointInt32(0.28) * x * x);
}
fixedPointInt32 fastAtan2(fixedPointInt32 fp1, fixedPointInt32 fp2) {
    if (fp1 == 0) // edge cases
        if (fp2 >= 0)
            return FAST_PI_2;
        else
            return -FAST_PI_2;
    if (fp1 > 0) return fastAtan(fp2 / fp1); //"Normal" case
    if (fp2 > 0)
        return fastAtan(fp2 / fp1) + FAST_PI; // less "normal" cases
    else
        return fastAtan(fp2 / fp1) - FAST_PI; // less "normal" cases
}
fixedPointInt32 fastCos(fixedPointInt32 x) {
    // taylor series approximation of cos(x) around 0, up to x^6
    // deviation: max 0.001, at +/- 0.5pi
    if (x < 0) x = -x;
    x %= FAST_PI << 1;
    fixedPointInt32 sign = 1;
    if (x > FAST_PI) {
        x -= FAST_PI;
        sign = -sign;
    }
    if (x > FAST_PI_2) {
        x    = FAST_PI - x;
        sign = -sign;
    }
    fixedPointInt32 out   = 1;
    fixedPointInt32 expo2 = x * x;
    fixedPointInt32 expo4 = expo2 * expo2;
    out -= expo2 >> 1;
    out += FACULTY_FOUR * expo4;
    out -= FACULTY_SIX * expo4 * expo2;
    return sign * out;
}
fixedPointInt32 fastSin(fixedPointInt32 x) { return fastCos(x - FAST_PI_2); }
fixedPointInt32 fastAcos(fixedPointInt32 x) {
    // https://stackoverflow.com/a/36387954/8807019
    fixedPointInt32 xsq = x * x;
    return FAST_PI_2 + (FAST_ACOS_A * x + FAST_ACOS_B * xsq * x) / (FAST_ACOS_C * xsq + FAST_ACOS_D * xsq * xsq + 1);
}
fixedPointInt32 fastAsin(fixedPointInt32 x) { return FAST_PI_2 - fastAcos(x); }
// Actually slower than normal root function:
/* fixedPointInt32 fastSqrt(fixedPointInt32 x) {
    fixedPointInt32 multiplier = 1;
    if (x < 0) x = -x;
    while (x > 4) {
        x          = x >> 2;
        multiplier = multiplier << 1;
    }
    while (x < 1) {
        x          = x << 2;
        multiplier = multiplier >> 1;
    }
    // taylor series around 2.3, constants from wolframalpha
    // maximum deviation: 1.01 (instead of 1) at x=1, 2.12 (instead of 2) at x=4
    fixedPointInt32 xConv  = x - TWO_POINT_THREE;
    fixedPointInt32 xConv2 = xConv;
    fixedPointInt32 out    = SQRT_A + SQRT_B * xConv2;
    xConv2 *= xConv;
    out -= SQRT_C * xConv2;
    xConv2 *= xConv;
    out += SQRT_D * xConv2;
    return out * multiplier;
}*/

void updateAttitude() {
    fixedPointInt32 gyroPitchFwd  = fixedPointInt32::fromRaw(gyroDataRaw[0]) * RAW_TO_RAD_PER_SEC;
    fixedPointInt32 gyroRollRight = fixedPointInt32::fromRaw(-gyroDataRaw[1]) * RAW_TO_RAD_PER_SEC;
    fixedPointInt32 gyroYawRight  = fixedPointInt32::fromRaw(-gyroDataRaw[2]) * RAW_TO_RAD_PER_SEC;

    fixedPointInt32 pitchFromGyro = pitch + gyroPitchFwd * FRAME_TIME;
    fixedPointInt32 rollFromGyro  = roll + gyroRollRight * FRAME_TIME;

    fixedPointInt32 accelRight = fixedPointInt32::fromRaw(-accelDataRaw[0]);
    fixedPointInt32 accelFwd   = fixedPointInt32::fromRaw(accelDataRaw[1]);
    fixedPointInt32 accelUp    = fixedPointInt32::fromRaw(accelDataRaw[2]);

    fixedPointInt32 len    = sqrtf((accelRight * accelRight + accelFwd * accelFwd + accelUp * accelUp).getFloat());
    fixedPointInt32 invlen = fixedPointInt32(1) / len;
    accelRight *= invlen;
    accelFwd *= invlen;
    accelUp *= invlen;
    fixedPointInt32 pitchFromAccel = fastAtan2(accelUp, accelFwd) * (accelFwd * accelFwd + accelUp * accelUp);
    fixedPointInt32 rollFromAccel  = fastAtan2(accelUp, -accelRight) * (accelRight * accelRight + accelUp * accelUp);
    fixedPointInt32 dRoll          = rollFromAccel - roll;
    fixedPointInt32 dPitch         = pitchFromAccel - pitch;
    if (dRoll > FAST_PI)
        rollFromAccel -= FAST_PI << 1;
    else if (dRoll < -FAST_PI)
        rollFromAccel += FAST_PI << 1;
    if (dPitch > FAST_PI)
        pitchFromAccel -= FAST_PI << 1;
    else if (dPitch < -FAST_PI)
        pitchFromAccel += FAST_PI << 1;

    fixedPointInt32 totalRotation = gyroPitchFwd * gyroPitchFwd + gyroRollRight * gyroRollRight + gyroYawRight * gyroYawRight;

    fixedPointInt32 alpha;
    if (len > INVALID_HIGH_THRES || len < INVALID_LOW_THRES || totalRotation > FAST_PI)
        alpha = 1;
    else if (totalRotation > QUARTER)
        alpha = map(totalRotation, QUARTER, FAST_PI, ALPHA_997, 1);
    else
        alpha = ALPHA_997;
    fixedPointInt32 beta = fixedPointInt32(1) - alpha;

    pitch = pitchFromGyro * alpha + pitchFromAccel * beta;
    roll  = rollFromGyro * alpha + rollFromAccel * beta;
    if (pitch > FAST_PI)
        pitch -= FAST_PI << 1;
    else if (pitch <= -FAST_PI)
        pitch += FAST_PI << 1;
    if (roll > FAST_PI)
        roll -= FAST_PI << 1;
    else if (roll <= -FAST_PI)
        roll += FAST_PI << 1;

    yaw = yaw + gyroYawRight * FRAME_TIME;
    if (yaw > FAST_PI) yaw -= FAST_PI << 1;
    if (yaw < -FAST_PI) yaw += FAST_PI << 1;
}