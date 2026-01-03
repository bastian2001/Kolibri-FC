#include "global.h"

// Partly taken from
// https://github.com/catphish/openuav/blob/master/firmware/src/imu.c

// COORDINATE SYSTEM:
// X: forward / roll right
// Y: right / pitch up
// Z: down / yaw right

// Applied in the order yaw -> pitch -> roll
// (rotate in horizontal plane -> how much to look up -> then roll right)

static constexpr f32 RAW_TO_RAD_PER_SEC = PI * 4000 / 65536 / 180; // 2000deg per second, but raw is only +/-.5
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
	magHeadingCorrection = PT1(magFilterCutoff, MAG_HARDWARE == MAG_HMC5883L ? 75 : 200);
	magHeadingCorrection.setRolloverParams(-FIX_PI, FIX_PI);

	combinedAltitudeFilter.set(baroASL);
}

void imuGyroUpdate() {
	// quaternion of all 3 axis rotations combined
	f32 dq[] = {gyroAligned[0] * RAW_TO_HALF_ANGLE, gyroAligned[1] * RAW_TO_HALF_ANGLE, gyroAligned[2] * RAW_TO_HALF_ANGLE};
	Quaternion buffer = q;
	q.w += -buffer.v[0] * dq[0] - buffer.v[1] * dq[1] - buffer.v[2] * dq[2];
	q.v[0] += +buffer.w * dq[0] + buffer.v[1] * dq[2] - buffer.v[2] * dq[1];
	q.v[1] += +buffer.w * dq[1] - buffer.v[0] * dq[2] + buffer.v[2] * dq[0];
	q.v[2] += +buffer.w * dq[2] + buffer.v[0] * dq[1] - buffer.v[1] * dq[0];

	Quaternion_normalize(&q, &q);
}

static Quaternion shortest_path;
void imuAccelUpdate1() {
	// Formula from http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/transforms/index.htm
	// p2.x = w*w*p1.x + 2*y*w*p1.z - 2*z*w*p1.y + x*x*p1.x + 2*y*x*p1.y + 2*z*x*p1.z - z*z*p1.x - y*y*p1.x;
	// p2.y = 2*x*y*p1.x + y*y*p1.y + 2*z*y*p1.z + 2*w*z*p1.x - z*z*p1.y + w*w*p1.y - 2*x*w*p1.z - x*x*p1.y;
	// p2.z = 2*x*z*p1.x + 2*y*z*p1.y + z*z*p1.z - 2*w*y*p1.x - y*y*p1.z + 2*w*x*p1.y - x*x*p1.z + w*w*p1.z;
	// with p1.x = 0, p1.y = 0, p1.z = -1, things can be simplified

	Quaternion con;
	Quaternion_conjugate(&q, &con);

	// world's down axis in quad's coordinate system
	f32 orientation_vector[3];
	f32 temp[3] = {0, 0, 1};
	Quaternion_rotate(&con, temp, orientation_vector);
	// orientation_vector[0] = con.w * con.v[1] * -2 + con.v[0] * con.v[2] * -2;
	// orientation_vector[1] = con.v[1] * con.v[2] * -2 + con.w * con.v[0] * 2;
	// orientation_vector[2] = -con.v[2] * con.v[2] + con.v[1] * con.v[1] + con.v[0] * con.v[0] - con.w * con.w;

	f32 accelVectorNorm = sqrtf(accelAligned[0] * accelAligned[0] +
								accelAligned[1] * accelAligned[1] +
								accelAligned[2] * accelAligned[2]);
	f32 accelVector[3];
	if (accelVectorNorm < 100) return; // throw out useless accel values, avoid division by 0
	f32 invAccelVectorNorm = 1 / accelVectorNorm;
	accelVector[0] = invAccelVectorNorm * accelAligned[0];
	accelVector[1] = invAccelVectorNorm * accelAligned[1];
	accelVector[2] = invAccelVectorNorm * accelAligned[2];

	// Accelerometer measures specific force (upward when only gravity acts).
	// Convert to a gravity/down estimate to match orientation_vector.
	f32 accelDown[3] = {accelVector[0], accelVector[1], accelVector[2]};
	// Rotate estimated down into measured accel; order matters (from estimate to measurement)
	Quaternion_from_unit_vecs(orientation_vector, accelDown, &shortest_path);
}

void imuAccelUpdate2() {
	f32 axis[3];
	f32 accAngle = Quaternion_toAxisAngle(&shortest_path, axis); // reduces effect of accel noise on attitude

	if (accAngle > ANGLE_CHANGE_LIMIT) accAngle = ANGLE_CHANGE_LIMIT;

	Quaternion c;
	// f32 c[3]; // correction quaternion, but w is 1
	Quaternion_fromAxisAngle(axis, accAngle, &c);
	// f32 co = accAngle * 0.5f;
	// c[0] = axis[0] * co;
	// c[1] = axis[1] * co;
	// c[2] = axis[2] * co;

	// Apply correction in body frame (right-multiply) so predicted down rotates toward measured down
	Quaternion_multiply(&q, &c, &q);
	// Quaternion buffer;
	// buffer.w = q.w - c[0] * q.v[0] - c[1] * q.v[1] - c[2] * q.v[2];
	// buffer.v[0] = c[0] * q.w + q.v[0] + c[1] * q.v[2] - c[2] * q.v[1];
	// buffer.v[1] = q.v[1] - c[0] * q.v[2] + c[1] * q.w + c[2] * q.v[0];
	// buffer.v[2] = q.v[2] + c[0] * q.v[1] - c[1] * q.v[0] + c[2] * q.w;

	Quaternion_normalize(&q, &q);
}

void imuUpdatePitchRoll() {
	startFixMath();
	roll = atan2Fix(2 * (q.w * q.v[0] + q.v[1] * q.v[2]), 1 - 2 * (q.v[0] * q.v[0] + q.v[1] * q.v[1]));
	pitch = asinf(constrain(2 * (q.w * q.v[1] - q.v[2] * q.v[0]), -1, 1));
	yaw = atan2Fix(2 * (q.w * q.v[2] + q.v[0] * q.v[1]), 1 - 2 * (q.v[1] * q.v[1] + q.v[2] * q.v[2]));
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

void imuUpdateSpeeds() {
	sinCosFix(roll, sinRoll, cosRoll);
	sinCosFix(pitch, sinPitch, cosPitch);
	sinCosFix(combinedHeading, sinHeading, cosHeading);
	vAccel = cosRoll * cosPitch * *accelFiltered[AXIS_YAW];
	vAccel += sinRoll * cosPitch * *accelFiltered[AXIS_PITCH];
	vAccel -= sinPitch * *accelFiltered[AXIS_ROLL];
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

	eVelFilter.add(eastAccel / (PID_FREQ / 8));
	nVelFilter.add(northAccel / (PID_FREQ / 8));
}
