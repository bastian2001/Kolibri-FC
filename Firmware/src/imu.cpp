#include "global.h"

// COORDINATE SYSTEM:
// X: forward / roll right
// Y: right / pitch up
// Z: down / yaw right
// (Tait-Bryan angles)

const float RAW_TO_RAD_PER_SEC = (float)(PI * 4000 / 65536 / 180); // 2000deg per second, but raw is only +/-.5
const float RAW_TO_M_PER_SEC2  = (float)(9.81 * 32 / 65536);	   // same reason, +/-16g
const float FRAME_TIME		   = (float)(1. / 3200);
const float RAW_TO_FINAL	   = (float)(RAW_TO_RAD_PER_SEC * FRAME_TIME);
const float ANGLE_CHANGE_LIMIT = .00004;

float pitch, roll, yaw;

Quaternion q;

void imuInit() {
	pitch  = 0; // pitch up
	roll   = 0; // roll right
	yaw	   = 0; // yaw right
	q.w	   = 1;
	q.v[0] = 0;
	q.v[1] = 0;
	q.v[2] = 0;
}

float map(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void updateFromGyro() {
	Quaternion q_x, q_y, q_z;
	float gyroRollRight = gyroDataRaw[1] * RAW_TO_FINAL;
	float gyroPitchUp	= gyroDataRaw[0] * RAW_TO_FINAL;
	float gyroYawRight	= gyroDataRaw[2] * RAW_TO_FINAL;

	// putting these two operations (fromXXXRotation and multiply) inline instead of the functions saves 45µs @132MHz at no loss in precision
	//  Quaternion_fromXRotation(gyroRollRight, &q_x);
	//  Quaternion_fromYRotation(gyroPitchUp, &q_y);
	//  Quaternion_fromZRotation(gyroYawRight, &q_z);
	// generate quaternions from axis-angle
	q_x = {gyroRollRight / 2, 0, 0, 1};
	q_y = {0, gyroPitchUp / 2, 0, 1};
	q_z = {0, 0, gyroYawRight / 2, 1};

	// Quaternion_multiply(&q_x, &q, &q);
	// Quaternion_multiply(&q_y, &q, &q);
	// Quaternion_multiply(&q_z, &q, &q);
	// multiply quaternions onto the attitude quaternion => inline saves time, because certain parts of the equation are known to be 0 and can therefore be omitted
	Quaternion result;
	result.w	= q_x.w * q.w - q_x.v[0] * q.v[0];
	result.v[0] = q_x.v[0] * q.w + q_x.w * q.v[0];
	result.v[1] = q_x.w * q.v[1] - q_x.v[0] * q.v[2];
	result.v[2] = q_x.w * q.v[2] + q_x.v[0] * q.v[1];
	q			= result;
	result.w	= q_y.w * q.w - q_y.v[1] * q.v[1];
	result.v[0] = q_y.w * q.v[0] + q_y.v[1] * q.v[2];
	result.v[1] = q_y.w * q.v[1] + q_y.v[1] * q.w;
	result.v[2] = q_y.w * q.v[2] - q_y.v[1] * q.v[0];
	q			= result;
	result.w	= q_z.w * q.w - q_z.v[2] * q.v[2];
	result.v[0] = q_z.w * q.v[0] - q_z.v[2] * q.v[1];
	result.v[1] = q_z.w * q.v[1] + q_z.v[2] * q.v[0];
	result.v[2] = q_z.w * q.v[2] + q_z.v[2] * q.w;
	q			= result;

	Quaternion_normalize(&q, &q);
}

float orientation_vector[3];
void updateFromAccel() {
	float vTemp[3] = {0, 0, -1};
	Quaternion_rotate(&q, vTemp, orientation_vector);

	float accelVector[3]  = {(float)-(accelDataRaw[1]), (float)-(accelDataRaw[0]), (float)-(accelDataRaw[2])};
	float accelVectorNorm = sqrtf((int)accelDataRaw[1] * (int)accelDataRaw[1] + (int)accelDataRaw[0] * (int)accelDataRaw[0] + (int)accelDataRaw[2] * (int)accelDataRaw[2]);
	if (accelVectorNorm > 0.01f) {
		float invAccelVectorNorm = 1 / accelVectorNorm;
		accelVector[0] *= invAccelVectorNorm;
		accelVector[1] *= invAccelVectorNorm;
		accelVector[2] *= invAccelVectorNorm;
	}
	Quaternion shortest_path;
	Quaternion_from_unit_vecs(orientation_vector, accelVector, &shortest_path);

	float axis[3];
	float angle = Quaternion_toAxisAngle(&shortest_path, axis) / 16; // reduces effect of accel noise on attitude

	if (angle > ANGLE_CHANGE_LIMIT) angle = ANGLE_CHANGE_LIMIT;

	Quaternion correction;
	Quaternion_fromAxisAngle(axis, angle, &correction);

	Quaternion_multiply(&correction, &q, &q);
	Quaternion_normalize(&q, &q);
}

void updatePitchRollValues() {
	Quaternion shortest_path;
	float vTemp[3] = {0, 0, -1};
	// decision was made to not recalculate the orientation_vector, as this saves 50µs @132MHz at only a slight loss in precision (accel update delayed by 1 cycle)
	Quaternion_from_unit_vecs(orientation_vector, vTemp, &shortest_path);
	float orientation_correction_axes[3];
	float angle = Quaternion_toAxisAngle(&shortest_path, orientation_correction_axes);
	roll		= orientation_correction_axes[0] * angle;
	pitch		= orientation_correction_axes[1] * angle;
	yaw			= atan2f(2.0 * (q.v[2] * q.w + q.v[1] * q.v[0]), 1 - 2 * (q.v[0] * q.v[0] + q.w * q.w));
}

void updateAttitude() {
	uint32_t t0, t1, t2;
	elapsedMicros timer = 0;
	updateFromGyro();
	t0 = timer;
	updateFromAccel();
	t1 = timer;
	updatePitchRollValues();
	t2 = timer;
	Serial.printf("Gyro: %3d, Accel: %3d, PitchRoll: %3d\n", t0, t1 - t0, t2 - t1);
}