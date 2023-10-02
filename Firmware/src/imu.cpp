#include "global.h"
// Reference:
// https://github.com/catphish/openuav/blob/master/firmware/src/imu.c

Quaternion q;

fixedPointInt32 gyroSign[3]  = {1, 1, 1};
fixedPointInt32 accelSign[3] = {1, 1, 1};

const fixedPointInt32 RAW_TO_RAD_PER_SEC = PI * 4000 / 180; // 2000deg per second, but raw is only +/-.5
const fixedPointInt32 RAW_TO_M_PER_SEC2  = 9.81 * 32;
const fixedPointInt32 FRAME_TIME         = fixedPointInt32::from(1) / fixedPointInt32::from(3200);
fixedPointInt32 ACCEL_UPDATE_EPS   = .0001;

void imuInit() {
    q.setIdentity();
}

void imuUpdateFromGyro() {
    fixedPointInt32 gyro_x = fixedPointInt32::fromRaw(gyroDataRaw[0]) * RAW_TO_RAD_PER_SEC * FRAME_TIME * gyroSign[0];
    fixedPointInt32 gyro_y = fixedPointInt32::fromRaw(gyroDataRaw[1]) * RAW_TO_RAD_PER_SEC * FRAME_TIME * gyroSign[1];
    fixedPointInt32 gyro_z = fixedPointInt32::fromRaw(gyroDataRaw[2]) * RAW_TO_RAD_PER_SEC * FRAME_TIME * gyroSign[2];

    Quaternion q_x, q_y, q_z;
    q_x.fromXRotation(gyro_x);
    q_y.fromYRotation(gyro_y);
    q_z.fromZRotation(gyro_z);

    q *= q_x;
    q *= q_y;
    q *= q_z;

    q.normalize();
}

void imuUpdateFromAccel() {
    fixedPointInt32 orientationVector[3];
    fixedPointInt32 v[3] = {0, 0, 1};
    q.rotate(v, orientationVector);

    // choosing upscaling to (arbitrary) m/s², to avoid precision loss during normalization (would happen if accelVectorLength < 1)
    fixedPointInt32 accelVector[3] = {
        fixedPointInt32::fromRaw(accelDataRaw[0]) * RAW_TO_M_PER_SEC2 * accelSign[0],
        fixedPointInt32::fromRaw(accelDataRaw[1]) * RAW_TO_M_PER_SEC2 * accelSign[1],
        fixedPointInt32::fromRaw(accelDataRaw[2]) * RAW_TO_M_PER_SEC2 * accelSign[2],
    };
    fixedPointInt32 accelVectorLength = sqrtf(vectorDot(accelVector, accelVector).getFloat());
    if (accelVectorLength == 0)
        accelVectorLength = 1;
    accelVector[0] /= accelVectorLength;
    accelVector[1] /= accelVectorLength;
    accelVector[2] /= accelVectorLength;

    Quaternion shortestPath;
    shortestPath.setFromUnitVecs(orientationVector, accelVector);

    fixedPointInt32 axis[3];
    fixedPointInt32 angle = shortestPath.toAxisAngle(axis);

    if (angle > ACCEL_UPDATE_EPS)
        angle = ACCEL_UPDATE_EPS;

    Quaternion correction;
    correction.fromAxisAngle(axis, angle);

    q = correction * q;
    q.normalize();
}

void imuGetXyTilt(fixedPointInt32 &pitch, fixedPointInt32 &roll) {
    fixedPointInt32 orientationVector[3];
    fixedPointInt32 v[3] = {0, 0, 1};
    q.rotate(v, orientationVector);

    Quaternion shortestPath;
    shortestPath.setFromUnitVecs(orientationVector, v);

    fixedPointInt32 orientationCorrectionAxes[3];
    fixedPointInt32 angle = shortestPath.toAxisAngle(orientationCorrectionAxes);

    pitch = orientationCorrectionAxes[0] * angle;
    roll  = orientationCorrectionAxes[1] * angle;
}