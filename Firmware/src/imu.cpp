#include "global.h"

const fixedPointInt32 FAST_PI            = PI;
const fixedPointInt32 FAST_PI_2          = PI / 2;
const fixedPointInt32 RAW_TO_RAD_PER_SEC = PI * 4000 / 180; // 2000deg per second, but raw is only +/-.5
const fixedPointInt32 RAW_TO_M_PER_SEC2  = 9.81 * 32;       // same reason, +/-16g
const fixedPointInt32 FRAME_TIME         = fixedPointInt32::from(1) / fixedPointInt32::from(3200);
const fixedPointInt32 FACULTY_FOUR       = 1. / 24.;
const fixedPointInt32 FACULTY_SIX        = 1. / 720.;
const fixedPointInt32 FAST_ACOS_A        = -0.939115566365855;
const fixedPointInt32 FAST_ACOS_B        = 0.9217841528914573;
const fixedPointInt32 FAST_ACOS_C        = -1.2845906244690837;
const fixedPointInt32 FAST_ACOS_D        = 0.295624144969963174;

// const fixedPointInt32 FILTER_ALPHA           = fixedPointInt32::from(.997);
// const fixedPointInt32 FILTER_ONE_MINUS_ALPHA = fixedPointInt32::from(1) - FILTER_ALPHA;

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

void updateAttitude() {

    fixedPointInt32 gyroPitchFwd  = fixedPointInt32::fromRaw(gyroDataRaw[0]) * RAW_TO_RAD_PER_SEC;
    fixedPointInt32 gyroRollRight = fixedPointInt32::fromRaw(-gyroDataRaw[1]) * RAW_TO_RAD_PER_SEC;
    fixedPointInt32 gyroYawRight  = fixedPointInt32::fromRaw(-gyroDataRaw[2]) * RAW_TO_RAD_PER_SEC;

    fixedPointInt32 pitchFromGyro = pitch;
    fixedPointInt32 rollFromGyro  = roll;

    pitchFromGyro += gyroPitchFwd * FRAME_TIME;
    rollFromGyro += gyroRollRight * FRAME_TIME;

    fixedPointInt32 sinY  = fastSin(gyroYawRight * FRAME_TIME);
    fixedPointInt32 sin2Y = sinY * sinY;

    // pitchFromGyro += sinY * roll;   // questionable, but works at the extremes (+/-90 deg rotation per frame and during no rotation)
    // rollFromGyro -= sinY * pitch;   // questionable too
    // pitchFromGyro -= sin2Y * pitch; // questionable too
    // rollFromGyro += sin2Y * roll;   // questionable too

    fixedPointInt32 accelRight = fixedPointInt32::fromRaw(-accelDataRaw[0]) * RAW_TO_M_PER_SEC2;
    fixedPointInt32 accelFwd   = fixedPointInt32::fromRaw(accelDataRaw[1]) * RAW_TO_M_PER_SEC2;
    fixedPointInt32 accelUp    = fixedPointInt32::fromRaw(accelDataRaw[2]) * RAW_TO_M_PER_SEC2;

    fixedPointInt32 len = sqrtf((accelRight * accelRight + accelFwd * accelFwd + accelUp * accelUp).getFloat());
    accelRight /= len;
    accelFwd /= len;
    accelUp /= len;
    fixedPointInt32 pitchFromAccel = fastAtan2(accelUp, accelFwd) * (accelFwd * accelFwd + accelUp * accelUp);
    fixedPointInt32 rollFromAccel  = fastAtan2(accelUp, -accelRight) * (accelRight * accelRight + accelUp * accelUp);

    fixedPointInt32 totalRotation = sqrtf((gyroPitchFwd * gyroPitchFwd + gyroRollRight * gyroRollRight + gyroYawRight * gyroYawRight).getFloat());

    fixedPointInt32 alpha;
    if (totalRotation > FAST_PI)
        alpha = 1;
    else if (totalRotation > 0.5)
        alpha = map(totalRotation, 0.5, FAST_PI, 0.997, 1);
    else
        alpha = 0.997;
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