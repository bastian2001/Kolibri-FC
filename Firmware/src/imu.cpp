#include "global.h"

// COORDINATE SYSTEM:
// X: forward / roll right
// Y: right / pitch up
// Z: down / yaw right
// (Tait-Bryan angles)

const float FAST_PI			   = PI;
const float FAST_PI_2		   = PI / 2;
const float RAW_TO_RAD_PER_SEC = (float)(PI * 4000 / 65536 / 180); // 2000deg per second, but raw is only +/-.5
const float RAW_TO_M_PER_SEC2  = (float)(9.81 * 32 / 65536);	   // same reason, +/-16g
const float FRAME_TIME		   = (float)(1. / 3200);
const float RAW_TO_FINAL	   = (float)(RAW_TO_RAD_PER_SEC * FRAME_TIME);
const float QUARTER			   = 0.25;
// const float ALPHA_997			 = 0.997;
// const float TWO_POINT_THREE	 = 2.3;
const float ANGLE_CHANGE_LIMIT = .0001;
// const float INVALID_LOW_THRES	 = float(9.6) / RAW_TO_M_PER_SEC2;
// const float INVALID_HIGH_THRES = float(10) / RAW_TO_M_PER_SEC2;

float pitch, roll, yaw;

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

float map(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void updateFromGyro() {
	float gyroRollRight = -gyroDataRaw[1] * RAW_TO_FINAL;
	float gyroPitchUp	= -gyroDataRaw[0] * RAW_TO_FINAL;
	float gyroYawRight	= -gyroDataRaw[2] * RAW_TO_FINAL;

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
	float accelForward = -accelDataRaw[1] * RAW_TO_M_PER_SEC2;
	float accelRight   = -accelDataRaw[0] * RAW_TO_M_PER_SEC2;
	float accelDown	   = -accelDataRaw[2] * RAW_TO_M_PER_SEC2;

	float orientation_vector[3];
	float vTemp[3] = {0, 0, -1};
	Quaternion_rotate(&attitude, vTemp, orientation_vector);

	float accelVector[3]  = {accelForward, accelRight, accelDown};
	float accelVectorNorm = sqrtf(accelRight * accelRight + accelForward * accelForward + accelDown * accelDown);
	if (accelVectorNorm < 0.01f) accelVectorNorm = 1;
	float invAccelVectorNorm = 1 / accelVectorNorm;
	accelVector[0] *= invAccelVectorNorm;
	accelVector[1] *= invAccelVectorNorm;
	accelVector[2] *= invAccelVectorNorm;

	Quaternion shortest_path;
	float	   new_orientation_vector[3];
	new_orientation_vector[0] = orientation_vector[0];
	new_orientation_vector[1] = orientation_vector[1];
	new_orientation_vector[2] = orientation_vector[2];
	Quaternion_from_unit_vecs(new_orientation_vector, accelVector, &shortest_path);

	float axis[3];
	float angle = Quaternion_toAxisAngle(&shortest_path, axis);
	if (angle > ANGLE_CHANGE_LIMIT) angle = ANGLE_CHANGE_LIMIT;

	Quaternion correction;
	Quaternion_fromAxisAngle(axis, angle, &correction);

	Quaternion_multiply(&correction, &attitude, &attitude);
	Quaternion_normalize(&attitude, &attitude);
}

void updatePitchRollValues() {
	float orientation_vector[3];
	float orientation_vector_raw[3] = {0, 0, -1};
	Quaternion_rotate(&attitude, orientation_vector_raw, orientation_vector);

	Quaternion shortest_path;
	float	   vTemp[3] = {0, 0, -1};
	float	   new_orientation_vector[3];
	new_orientation_vector[0] = orientation_vector[0];
	new_orientation_vector[1] = orientation_vector[1];
	new_orientation_vector[2] = orientation_vector[2];
	Quaternion_from_unit_vecs(new_orientation_vector, vTemp, &shortest_path);

	float orientation_correction_axes[3];
	float angle = Quaternion_toAxisAngle(&shortest_path, orientation_correction_axes);

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
	// Serial.printf("Gyro: %d, Accel: %d, PitchRoll: %d\n", t0, t1 - t0, t2 - t1);
}

/*void updateAttitude() {
	float gyroPitchFwd  = float::fromRaw(gyroDataRaw[0]) * RAW_TO_RAD_PER_SEC;
	float gyroRollRight = float::fromRaw(-gyroDataRaw[1]) * RAW_TO_RAD_PER_SEC;
	float gyroYawRight  = float::fromRaw(-gyroDataRaw[2]) * RAW_TO_RAD_PER_SEC;

	float pitchFromGyro = pitch + gyroPitchFwd * FRAME_TIME;
	float rollFromGyro  = roll + gyroRollRight * FRAME_TIME;

	float accelRight = float::fromRaw(-accelDataRaw[0]);
	float accelFwd   = float::fromRaw(accelDataRaw[1]);
	float accelUp	   = float::fromRaw(accelDataRaw[2]);

	float len	   = sqrtf((accelRight * accelRight + accelFwd * accelFwd + accelUp * accelUp).getFloat());
	float invlen = float(1) / len;
	accelRight *= invlen;
	accelFwd *= invlen;
	accelUp *= invlen;
	float pitchFromAccel = fastAtan2(accelUp, accelFwd) * (accelFwd * accelFwd + accelUp * accelUp);
	float rollFromAccel  = fastAtan2(accelUp, -accelRight) * (accelRight * accelRight + accelUp * accelUp);
	float dRoll		   = rollFromAccel - roll;
	float dPitch		   = pitchFromAccel - pitch;
	if (dRoll > FAST_PI)
		rollFromAccel -= FAST_PI << 1;
	else if (dRoll < -FAST_PI)
		rollFromAccel += FAST_PI << 1;
	if (dPitch > FAST_PI)
		pitchFromAccel -= FAST_PI << 1;
	else if (dPitch < -FAST_PI)
		pitchFromAccel += FAST_PI << 1;

	float totalRotation = gyroPitchFwd * gyroPitchFwd + gyroRollRight * gyroRollRight + gyroYawRight * gyroYawRight;

	float alpha;
	if (len > INVALID_HIGH_THRES || len < INVALID_LOW_THRES || totalRotation > FAST_PI)
		alpha = 1;
	else if (totalRotation > QUARTER)
		alpha = map(totalRotation, QUARTER, FAST_PI, ALPHA_997, 1);
	else
		alpha = ALPHA_997;
	float beta = float(1) - alpha;

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