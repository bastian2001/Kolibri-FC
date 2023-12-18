#include "global.h"

// COORDINATE SYSTEM:
// X: forward / roll right
// Y: right / pitch up
// Z: down / yaw right
// (Tait-Bryan angles)

const fixedPointInt32 FAST_PI			 = PI;
const fixedPointInt32 FAST_PI_2			 = PI / 2;
const fixedPointInt64 RAW_TO_RAD_PER_SEC = (float)(PI * 4000 / 180 * 65536); // 2000deg per second, but raw is only +/-.5
const fixedPointInt64 RAW_TO_M_PER_SEC2	 = (float)(9.81 * 32 * 65536);		 // same reason, +/-16g
const fixedPointInt64 FRAME_TIME		 = (float)(1. / 3200);
const fixedPointInt32 QUARTER			 = 0.25;
// const fixedPointInt32 ALPHA_997			 = 0.997;
// const fixedPointInt32 TWO_POINT_THREE	 = 2.3;
const fixedPointInt32 ANGLE_CHANGE_LIMIT = .0001;
// const fixedPointInt32 INVALID_LOW_THRES	 = fixedPointInt32(9.6) / RAW_TO_M_PER_SEC2;
// const fixedPointInt32 INVALID_HIGH_THRES = fixedPointInt32(10) / RAW_TO_M_PER_SEC2;

fixedPointInt32 pitch, roll, yaw;

Quaternion attitude;

void imuInit() {
	pitch		  = 0; // pitch up
	roll		  = 0; // roll right
	yaw			  = 0; // yaw right
	attitude.w	  = 1;
	attitude.v[0] = 0;
	attitude.v[1] = 0;
	attitude.v[2] = 0;
}

fixedPointInt32 map(fixedPointInt32 x, fixedPointInt32 in_min, fixedPointInt32 in_max, fixedPointInt32 out_min, fixedPointInt32 out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void updateFromGyro() {
	fixedPointInt64 gyroRollRight = fixedPointInt64::fromRaw(-gyroDataRaw[1]) * RAW_TO_RAD_PER_SEC * FRAME_TIME;
	fixedPointInt64 gyroPitchUp	  = fixedPointInt64::fromRaw(-gyroDataRaw[0]) * RAW_TO_RAD_PER_SEC * FRAME_TIME;
	fixedPointInt64 gyroYawRight  = fixedPointInt64::fromRaw(-gyroDataRaw[2]) * RAW_TO_RAD_PER_SEC * FRAME_TIME;

	Quaternion q_x, q_y, q_z;
	Quaternion_fromXRotation(gyroRollRight, &q_x);
	Quaternion_fromYRotation(gyroPitchUp, &q_y);
	Quaternion_fromZRotation(gyroYawRight, &q_z);

	Quaternion_multiply(&q_x, &attitude, &attitude);
	Quaternion_multiply(&q_y, &attitude, &attitude);
	Quaternion_multiply(&q_z, &attitude, &attitude);

	Quaternion_normalize(&attitude, &attitude);
}

void updateFromAccel() {
	fixedPointInt64 accelForward = fixedPointInt64::fromRaw(-accelDataRaw[1]) * RAW_TO_M_PER_SEC2;
	fixedPointInt64 accelRight	 = fixedPointInt64::fromRaw(-accelDataRaw[0]) * RAW_TO_M_PER_SEC2;
	fixedPointInt64 accelDown	 = fixedPointInt64::fromRaw(-accelDataRaw[2]) * RAW_TO_M_PER_SEC2;

	fixedPointInt64 orientation_vector[3];
	fixedPointInt64 vTemp[3] = {0, 0, -1};
	Quaternion_rotate(&attitude, vTemp, orientation_vector);

	fixedPointInt32 accelVector[3]	= {accelForward.toFixed32(), accelRight.toFixed32(), accelDown.toFixed32()};
	fixedPointInt32 accelVectorNorm = sqrtf((accelRight * accelRight + accelForward * accelForward + accelDown * accelDown).getFloat());
	if (accelVectorNorm < 0.01) accelVectorNorm = 1;
	fixedPointInt32 invAccelVectorNorm = fixedPointInt32(1) / accelVectorNorm;
	accelVector[0] *= invAccelVectorNorm;
	accelVector[1] *= invAccelVectorNorm;
	accelVector[2] *= invAccelVectorNorm;

	Quaternion		shortest_path;
	fixedPointInt32 new_orientation_vector[3];
	new_orientation_vector[0] = orientation_vector[0].toFixed32();
	new_orientation_vector[1] = orientation_vector[1].toFixed32();
	new_orientation_vector[2] = orientation_vector[2].toFixed32();
	Quaternion_from_unit_vecs(new_orientation_vector, accelVector, &shortest_path);

	fixedPointInt32 axis[3];
	fixedPointInt32 angle = Quaternion_toAxisAngle(&shortest_path, axis);
	if (angle > ANGLE_CHANGE_LIMIT) angle = ANGLE_CHANGE_LIMIT;

	Quaternion correction;
	Quaternion_fromAxisAngle(axis, angle.toFixed64(), &correction);

	Quaternion_multiply(&correction, &attitude, &attitude);
	Quaternion_normalize(&attitude, &attitude);
}

void updatePitchRollValues() {
	fixedPointInt64 orientation_vector[3];
	fixedPointInt64 orientation_vector_raw[3] = {0, 0, -1};
	Quaternion_rotate(&attitude, orientation_vector_raw, orientation_vector);

	Quaternion		shortest_path;
	fixedPointInt32 vTemp[3] = {0, 0, -1};
	fixedPointInt32 new_orientation_vector[3];
	new_orientation_vector[0] = orientation_vector[0].toFixed32();
	new_orientation_vector[1] = orientation_vector[1].toFixed32();
	new_orientation_vector[2] = orientation_vector[2].toFixed32();
	Quaternion_from_unit_vecs(new_orientation_vector, vTemp, &shortest_path);

	fixedPointInt32 orientation_correction_axes[3];
	fixedPointInt32 angle = Quaternion_toAxisAngle(&shortest_path, orientation_correction_axes);

	pitch = orientation_correction_axes[1] * angle;
	roll  = orientation_correction_axes[0] * angle;
}

void updateAttitude() {
	elapsedMicros timer = 0;
	uint32_t	  t0, t1, t2;
	updateFromGyro();
	t0 = timer;
	updateFromAccel();
	t1 = timer;
	updatePitchRollValues();
	t2 = timer;
	Serial.printf("Gyro: %d, Accel: %d, PitchRoll: %d\n", t0, t1 - t0, t2 - t1);
}

/*void updateAttitude() {
	fixedPointInt32 gyroPitchFwd  = fixedPointInt32::fromRaw(gyroDataRaw[0]) * RAW_TO_RAD_PER_SEC;
	fixedPointInt32 gyroRollRight = fixedPointInt32::fromRaw(-gyroDataRaw[1]) * RAW_TO_RAD_PER_SEC;
	fixedPointInt32 gyroYawRight  = fixedPointInt32::fromRaw(-gyroDataRaw[2]) * RAW_TO_RAD_PER_SEC;

	fixedPointInt32 pitchFromGyro = pitch + gyroPitchFwd * FRAME_TIME;
	fixedPointInt32 rollFromGyro  = roll + gyroRollRight * FRAME_TIME;

	fixedPointInt32 accelRight = fixedPointInt32::fromRaw(-accelDataRaw[0]);
	fixedPointInt32 accelFwd   = fixedPointInt32::fromRaw(accelDataRaw[1]);
	fixedPointInt32 accelUp	   = fixedPointInt32::fromRaw(accelDataRaw[2]);

	fixedPointInt32 len	   = sqrtf((accelRight * accelRight + accelFwd * accelFwd + accelUp * accelUp).getFloat());
	fixedPointInt32 invlen = fixedPointInt32(1) / len;
	accelRight *= invlen;
	accelFwd *= invlen;
	accelUp *= invlen;
	fixedPointInt32 pitchFromAccel = fastAtan2(accelUp, accelFwd) * (accelFwd * accelFwd + accelUp * accelUp);
	fixedPointInt32 rollFromAccel  = fastAtan2(accelUp, -accelRight) * (accelRight * accelRight + accelUp * accelUp);
	fixedPointInt32 dRoll		   = rollFromAccel - roll;
	fixedPointInt32 dPitch		   = pitchFromAccel - pitch;
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
}*/