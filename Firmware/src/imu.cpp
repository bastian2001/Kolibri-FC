#include "global.h"

// COORDINATE SYSTEM:
// X: forward / roll right
// Y: right / pitch up
// Z: down / yaw right
// (Tait-Bryan angles)

const float RAW_TO_RAD_PER_SEC = (float)(PI * 4000 / 65536 / 180); // 2000deg per second, but raw is only +/-.5
const float RAW_TO_M_PER_SEC2  = (float)(9.81 * 32 / 65536);	   // same reason, +/-16g
const float FRAME_TIME		   = (float)(1. / 3200);
const float RAW_TO_HALF_ANGLE  = (float)(RAW_TO_RAD_PER_SEC * FRAME_TIME / 2);
const float ANGLE_CHANGE_LIMIT = .00004;

float pitch, roll, yaw;
int32_t headMotAtt; // heading of motion by attitude, i.e. yaw but with pitch/roll compensation
int32_t combinedHeading;

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
	// quaternion of all 3 axis rotations combined. All the other terms are dependent on the order of multiplication, therefore we leave them out, as they are arbitrarily defined
	// e.g. defining a q_x, q_y and q_z and then multiplying them in a certain order would result in a different quaternion than multiplying them in a different order. Therefore we omit these inconsistent terms completely
	crashInfo[133] = 1;
	Quaternion all = {gyroDataRaw[1] * RAW_TO_HALF_ANGLE, gyroDataRaw[0] * RAW_TO_HALF_ANGLE, gyroDataRaw[2] * RAW_TO_HALF_ANGLE, 1};
	crashInfo[133] = 2;

	Quaternion_multiply(&all, &q, &q);
	crashInfo[133] = 3;

	Quaternion_normalize(&q, &q);
	crashInfo[133] = 4;
}

float orientation_vector[3];
void updateFromAccel() {
	// Formula from http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/transforms/index.htm
	// p2.x = w*w*p1.x + 2*y*w*p1.z - 2*z*w*p1.y + x*x*p1.x + 2*y*x*p1.y + 2*z*x*p1.z - z*z*p1.x - y*y*p1.x;
	// p2.y = 2*x*y*p1.x + y*y*p1.y + 2*z*y*p1.z + 2*w*z*p1.x - z*z*p1.y + w*w*p1.y - 2*x*w*p1.z - x*x*p1.y;
	// p2.z = 2*x*z*p1.x + 2*y*z*p1.y + z*z*p1.z - 2*w*y*p1.x - y*y*p1.z + 2*w*x*p1.y - x*x*p1.z + w*w*p1.z;
	// with p1.x = 0, p1.y = 0, p1.z = -1, things can be simplified
	crashInfo[134] = 1;

	orientation_vector[0] = q.w * q.v[1] * -2 + q.v[0] * q.v[2] * -2;
	orientation_vector[1] = q.v[1] * q.v[2] * -2 + q.w * q.v[0] * 2;
	orientation_vector[2] = -q.v[2] * q.v[2] + q.v[1] * q.v[1] + q.v[0] * q.v[0] - q.w * q.w;

	float accelVector[3]  = {(float)-(accelDataRaw[1]), (float)-(accelDataRaw[0]), (float)-(accelDataRaw[2])};
	float accelVectorNorm = sqrtf((int)accelDataRaw[1] * (int)accelDataRaw[1] + (int)accelDataRaw[0] * (int)accelDataRaw[0] + (int)accelDataRaw[2] * (int)accelDataRaw[2]);
	crashInfo[134]		  = 2;
	if (accelVectorNorm > 0.01f) {
		crashInfo[134]			 = 3;
		float invAccelVectorNorm = 1 / accelVectorNorm;
		accelVector[0] *= invAccelVectorNorm;
		accelVector[1] *= invAccelVectorNorm;
		accelVector[2] *= invAccelVectorNorm;
	}
	crashInfo[134] = 4;
	Quaternion shortest_path;
	Quaternion_from_unit_vecs(orientation_vector, accelVector, &shortest_path);
	crashInfo[134] = 5;

	float axis[3];
	float angle	   = Quaternion_toAxisAngle(&shortest_path, axis) / 2048; // reduces effect of accel noise on attitude
	crashInfo[134] = 6;

	if (angle > ANGLE_CHANGE_LIMIT) angle = ANGLE_CHANGE_LIMIT;
	crashInfo[134] = 7;

	Quaternion correction;
	Quaternion_fromAxisAngle(axis, angle, &correction);
	crashInfo[134] = 8;

	Quaternion_multiply(&correction, &q, &q);
	crashInfo[134] = 9;
	Quaternion_normalize(&q, &q);
	crashInfo[134] = 10;
}

void updatePitchRollValues() {
	crashInfo[135] = 1;
	Quaternion shortest_path;
	// decision was made to not recalculate the orientation_vector, as this saves 50µs @132MHz at only a slight loss in precision (accel update delayed by 1 cycle)

	float dot = -orientation_vector[2];

	if (dot > ONE_MINUS_EPS) {
		crashInfo[135] = 2;
		Quaternion_setIdentity(&shortest_path);
		crashInfo[135] = 3;
	} else if (dot < -ONE_MINUS_EPS) {
		crashInfo[135] = 4;
		// Rotate along any orthonormal vec to vec1 or vec2 as the axis.
		shortest_path.w	   = 0;
		shortest_path.v[0] = 0;
		shortest_path.v[1] = -orientation_vector[2];
		shortest_path.v[2] = orientation_vector[1];
		crashInfo[135]	   = 5;
	} else {
		crashInfo[135]	   = 6;
		shortest_path.w	   = dot + 1;
		shortest_path.v[0] = -orientation_vector[1];
		shortest_path.v[1] = orientation_vector[0];
		shortest_path.v[2] = 0;
		float len		   = sqrtf(shortest_path.v[0] * shortest_path.v[0] + shortest_path.v[1] * shortest_path.v[1] + shortest_path.w * shortest_path.w);
		crashInfo[135]	   = 7;
		if (len == 0) {
			crashInfo[135]	   = 8;
			shortest_path.w	   = 1;
			shortest_path.v[0] = 0;
			shortest_path.v[1] = 0;
			shortest_path.v[2] = 0;
			crashInfo[135]	   = 9;
		} else {
			crashInfo[135]	 = 10;
			float oneOverLen = 1 / len;
			shortest_path.w *= oneOverLen;
			shortest_path.v[0] *= oneOverLen;
			shortest_path.v[1] *= oneOverLen;
			shortest_path.v[2] = 0;
			crashInfo[135]	   = 11;
		}
	}

	float orientation_correction_axes[3];
	crashInfo[135] = 12;
	float angle	   = Quaternion_toAxisAngle(&shortest_path, orientation_correction_axes);
	crashInfo[135] = 13;

	roll  = orientation_correction_axes[0] * angle;
	pitch = orientation_correction_axes[1] * angle;
	yaw	  = atan2f(-2 * (q.v[2] * q.w + q.v[1] * q.v[0]), -1 + 2 * (q.v[0] * q.v[0] + q.w * q.w));

	headMotAtt = yaw * 5729578; // 5729578 = 360 / (2 * PI) * 100000
	if (angle > .2618f) {
		// assume the quad is flying into the direction of pitch and roll
		headMotAtt += atan2f(-orientation_vector[1], -orientation_vector[0]) * 5729578.f;
	}
	combinedHeading = headMotAtt + headingAdjustment;
	crashInfo[135]	= 14;
}

void updateAttitude() {
	crashInfo[132] = 1;
	uint32_t t0, t1, t2;
	elapsedMicros timer = 0;
	crashInfo[132]		= 2;
	updateFromGyro();
	crashInfo[132] = 3;
	t0			   = timer;
	updateFromAccel();
	crashInfo[132] = 4;
	t1			   = timer;
	updatePitchRollValues();
	crashInfo[132] = 5;
	t2			   = timer;
	// Serial.printf("Gyro: %3d, Accel: %3d, PitchRoll: %3d\n", t0, t1 - t0, t2 - t1);
}