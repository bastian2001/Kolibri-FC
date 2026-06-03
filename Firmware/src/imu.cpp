/**
 * @file imu.cpp
 * @brief Implementation of IMU reading and sensor fusion (complementary filter of gyro and accel, based on quaternions)
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"

// Partly taken from
// https://github.com/catphish/openuav/blob/master/firmware/src/imu.c

// COORDINATE SYSTEM:
// X: forward / roll right
// Y: right / pitch up
// Z: down / yaw right

// Applied in the order yaw -> pitch -> roll
// (rotate in horizontal plane -> how much to look up -> then roll right)

static constexpr f32 RAW_TO_RAD_PER_SEC = 70 * .001 * M_PI / 180; // LSM6DSV has a weird sensitivity
// static constexpr f32 RAW_TO_RAD_PER_SEC = PI * 4000 / 65536 / 180; // 2000deg per second, but raw is only +/-.5
static constexpr f32 FRAME_TIME = 1. / PID_FREQ;
static constexpr f32 RAW_TO_HALF_ANGLE = RAW_TO_RAD_PER_SEC * FRAME_TIME / 2;
static constexpr f32 ANGLE_CHANGE_LIMIT = 0.2 * (8 * FRAME_TIME); // 0.2 rad per second
fix32 accelFilterCutoff;
fix32 roll, pitch, yaw;
fix32 combinedHeading; // heading of quad, NOT heading of motion
fix32 cosPitch, cosRoll, sinPitch, sinRoll, cosHeading, sinHeading;
PT1 magHeadingCorrection;
fix32 magFilterCutoff;
static DualPT1 vVelFilter(0.1, PID_FREQ / 8), combinedAltitudeFilter(0.03, PID_FREQ / 8);
const fix32 &combinedAltitude = combinedAltitudeFilter.getConstRef();
const fix32 &vVel = vVelFilter.getConstRef();
PT1 baroImuUpVelFilter(0.2, 50);
static PT1 baroImuUpVelFilter2(5, PID_FREQ / 8);
static fix32 lastBaroImuUpVel;
PT1 eVelFilter;
PT1 nVelFilter;
const fix32 &eVel = eVelFilter.getConstRef();
const fix32 &nVel = nVelFilter.getConstRef();
fix32 vAccel;
volatile u8 altInitState = 0;

Quaternion q;

void imuInit() {
	addSetting(SETTING_GPS_VEL_FILTER_CUTOFF, &gpsVelocityFilterCutoff, 0.2f);
	addSetting(SETTING_GPS_UPDATE_RATE, &gpsUpdateRate, 20);
	addSetting(SETTING_MAG_FILTER_CUTOFF, &magFilterCutoff, 0.02f);

	roll = 0; // roll right
	pitch = 0; // pitch up
	yaw = 0; // yaw right
	q.w = 1;
	q.v[0] = 0;
	q.v[1] = 0;
	q.v[2] = 0;
	eVelFilter = PT1(gpsVelocityFilterCutoff, gpsUpdateRate);
	nVelFilter = PT1(gpsVelocityFilterCutoff, gpsUpdateRate);
	magHeadingCorrection = PT1(magFilterCutoff, HW_MAG == MAG_HMC5883L ? 75 : 200);
	magHeadingCorrection.setRolloverParams(-FIX_PI, FIX_PI);

	combinedAltitudeFilter.set(baroASL);
	Quaternion_setIdentity(&q);
}

static inline void imuGyroUpdate() {
	f32 dq[] = {gyroAligned[0] * RAW_TO_HALF_ANGLE, gyroAligned[1] * RAW_TO_HALF_ANGLE, gyroAligned[2] * RAW_TO_HALF_ANGLE};
	Quaternion temp = q;

	q.w += -temp.v[0] * dq[0] - temp.v[1] * dq[1] - temp.v[2] * dq[2];
	q.v[0] += +temp.w * dq[0] + temp.v[1] * dq[2] - temp.v[2] * dq[1];
	q.v[1] += +temp.w * dq[1] - temp.v[0] * dq[2] + temp.v[2] * dq[0];
	q.v[2] += +temp.w * dq[2] + temp.v[0] * dq[1] - temp.v[1] * dq[0];

	// The above is identical to:
	// Quaternion dq;
	// small angle approximation with sin(x) = x, cos(x) = 1
	// dq.w = 1; dq.v[0..2] = gyroAligned[0..2] * RAW_TO_HALF_ANGLE;
	// Quaternion_multiply(&q, &dq, &q);

	Quaternion_normalize_fast(&q);
}

static Quaternion shortest_path = {0, 0, 0, 1};
static inline void imuAccelUpdate1() {
	// Formula from http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/transforms/index.htm
	// p2.x = w*w*p1.x + 2*y*w*p1.z - 2*z*w*p1.y + x*x*p1.x + 2*y*x*p1.y + 2*z*x*p1.z - z*z*p1.x - y*y*p1.x;
	// p2.y = 2*x*y*p1.x + y*y*p1.y + 2*z*y*p1.z + 2*w*z*p1.x - z*z*p1.y + w*w*p1.y - 2*x*w*p1.z - x*x*p1.y;
	// p2.z = 2*x*z*p1.x + 2*y*z*p1.y + z*z*p1.z - 2*w*y*p1.x - y*y*p1.z + 2*w*x*p1.y - x*x*p1.z + w*w*p1.z;
	// with p1.x = 0, p1.y = 0, p1.z = -1, things can be simplified

	f32 upBody[3];

	// When stationary (which is on average the case), the measured acceleration points against gravity
	// In world frame: gravity is (0,0,1) down, so specific force is (0,0,-1).
	// q is body->world, so use q* to bring that world vector into body.
	upBody[0] = q.w * q.v[1] * 2 - q.v[0] * q.v[2] * 2;
	upBody[1] = q.v[1] * q.v[2] * -2 - q.w * q.v[0] * 2;
	upBody[2] = q.v[1] * q.v[1] - q.v[2] * q.v[2] + q.v[0] * q.v[0] - q.w * q.w;

	// The above is identical to:
	// f32 upWorld[3] = {0, 0, -1};
	// Quaternion q_conj;
	// Quaternion_conjugate(&q, &q_conj);
	// Quaternion_rotate(&conj, upWorld, upBody);

	startFixMath();
	f32 accelLength = (sqrtFix(fix32().setRaw(accelAligned[0] * accelAligned[0] + accelAligned[1] * accelAligned[1] + accelAligned[2] * accelAligned[2])) << 8).getf32();
	if (accelLength < 100) { // throw out useless accel values, avoid division by 0
		Quaternion_setIdentity(&shortest_path);
		return;
	}
	f32 accelVector[3];
	f32 accelLengthRec = 1 / accelLength;
	accelVector[0] = accelLengthRec * accelAligned[0];
	accelVector[1] = accelLengthRec * accelAligned[1];
	accelVector[2] = accelLengthRec * accelAligned[2];

	Quaternion_from_unit_vecs(accelVector, upBody, &shortest_path);
}

static inline void imuAccelUpdate2() {
	// We limit the correction angle so the accel can only slowly correct the attitude over time
	f32 axis[3];
	f32 accAngle = Quaternion_toAxisAngle(&shortest_path, axis) / 128;

	if (accAngle > ANGLE_CHANGE_LIMIT) accAngle = ANGLE_CHANGE_LIMIT;

	f32 c[3]; // correction quaternion, but w is 1
	f32 co = accAngle * 0.5f; // assume sin(a / 2) = a / 2
	c[0] = axis[0] * co;
	c[1] = axis[1] * co;
	c[2] = axis[2] * co;

	Quaternion temp = q;
	q.w -= temp.v[0] * c[0] + temp.v[1] * c[1] + temp.v[2] * c[2];
	q.v[0] += temp.w * c[0] + temp.v[1] * c[2] - temp.v[2] * c[1];
	q.v[1] += temp.w * c[1] - temp.v[0] * c[2] + temp.v[2] * c[0];
	q.v[2] += temp.w * c[2] + temp.v[0] * c[1] - temp.v[1] * c[0];

	// The above is equivalent to:
	// Not 100% identical because small angle approximation is used above
	// Quaternion c;
	// Quaternion_fromAxisAngle(axis, accAngle, &c);
	// Quaternion_multiply(&q, &c, &q);

	Quaternion_normalize_fast(&q);
}

static inline void imuUpdatePitchRoll() {
	roll = atan2f(2 * (q.w * q.v[0] + q.v[1] * q.v[2]), 1 - 2 * (q.v[0] * q.v[0] + q.v[1] * q.v[1]));
	pitch = asinf(constrain(2 * (q.w * q.v[1] - q.v[2] * q.v[0]), -1, 1));
	yaw = atan2f(2 * (q.w * q.v[2] + q.v[0] * q.v[1]), 1 - 2 * (q.v[1] * q.v[1] + q.v[2] * q.v[2]));
	fix32 temp = yaw + magHeadingCorrection;
	if (temp >= FIX_PI) {
		temp -= FIX_PI * 2;
	} else if (temp < -FIX_PI) {
		temp += FIX_PI * 2;
	}
	combinedHeading = temp;
}

static fix32 rAccel, fAccel;
static fix32 nAccel, eAccel;
static u8 lastAltInitState = 0;

static inline void imuUpdateSpeeds() {
	startFixMath();
	sinCosFix(roll, sinRoll, cosRoll);
	sinCosFix(pitch, sinPitch, cosPitch);
	sinCosFix(combinedHeading, sinHeading, cosHeading);
	vAccel = -cosRoll * cosPitch * *accelFiltered[AXIS_YAW];
	vAccel -= sinRoll * cosPitch * *accelFiltered[AXIS_PITCH];
	vAccel += sinPitch * *accelFiltered[AXIS_ROLL];
	vAccel -= 9.81f; // remove gravity
	if (altInitState > lastAltInitState) {
		lastAltInitState = altInitState;
		lastBaroImuUpVel = 0;
		baroImuUpVelFilter2.set(0);
		vVelFilter.set(0);
		combinedAltitudeFilter.set(gpsBaroAlt);
	} else {
		baroImuUpVelFilter.add(vAccel / (PID_FREQ / 8));
		lastBaroImuUpVel = baroImuUpVelFilter2;
		const fix32 baroImuUpAccel = baroImuUpVelFilter2.update(baroImuUpVelFilter) - lastBaroImuUpVel;
		vVelFilter.add(baroImuUpAccel);
		const fix32 filterVel = gpsGoodQuality ? fix32(-gpsMotion.velD / 10) * 0.01f : baroUpVel;
		combinedAltitudeFilter.add(vVelFilter.update(filterVel) / (PID_FREQ / 8));
		combinedAltitudeFilter.update(gpsBaroAlt);
	}
	mspDebugSensors[2] = (vVel * 10000).geti32();

	const fix32 rightAccel = cosRoll * *accelFiltered[AXIS_PITCH] - sinRoll * *accelFiltered[AXIS_YAW];
	const fix32 forwardAccel = cosPitch * *accelFiltered[AXIS_ROLL] + sinPitch * sinRoll * *accelFiltered[AXIS_PITCH] + sinPitch * cosRoll * *accelFiltered[AXIS_YAW];
	const fix32 northAccel = forwardAccel * cosHeading - rightAccel * sinHeading;
	const fix32 eastAccel = rightAccel * cosHeading + forwardAccel * sinHeading;
	rAccel = rightAccel;
	fAccel = forwardAccel;
	nAccel = northAccel;
	eAccel = eastAccel;
	mspDebugSensors[0] = (nAccel * 100).geti32();
	mspDebugSensors[3] = (eAccel * 256).geti32();

	eVelFilter.add(eastAccel / (PID_FREQ / 8));
	nVelFilter.add(northAccel / (PID_FREQ / 8));
}

void __not_in_flash_func(imuLoop)() {
	static u8 imuUpdateCycle = 0;
	TASK_START(TASK_IMU);
	TASK_START(TASK_IMU_GYRO);
	imuGyroUpdate();
	TASK_END(TASK_IMU_GYRO);

	switch (imuUpdateCycle) {
	case 0: {
		TASK_START(TASK_IMU_ACCEL1);
		imuAccelUpdate1();
		TASK_END(TASK_IMU_ACCEL1);
	} break;
	case 1: {
		TASK_START(TASK_IMU_ACCEL2);
		imuAccelUpdate2();
		TASK_END(TASK_IMU_ACCEL2);
	} break;
	case 2: {
		TASK_START(TASK_IMU_ANGLE);
		imuUpdatePitchRoll();
		TASK_END(TASK_IMU_ANGLE);
	} break;
	case 3: {
		TASK_START(TASK_IMU_SPEEDS);
		imuUpdateSpeeds();
		TASK_END(TASK_IMU_SPEEDS);
	} break;
	}
	if (++imuUpdateCycle >= 8) imuUpdateCycle = 0;
	TASK_END(TASK_IMU);
}
