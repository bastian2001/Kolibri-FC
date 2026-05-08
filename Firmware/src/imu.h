/**
 * @file imu.h
 * @brief Interface with the IMU (gyro and accel) and sensor fusion implementation
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

#include "utils/filters.h"
#include "utils/quaternion.h"
#include <Arduino.h>
#include <fixedPointInt.h>

extern fix32 roll, pitch, yaw; // Euler angles of the drone, roll right, pitch up, yaw right
extern fix32 accelFilterCutoff; // filter frequency for the accelerometer data
extern fix32 combinedHeading; // heading of the drone (in rad) by combining the magnetometer and the gyro
extern fix32 cosPitch, cosRoll, sinPitch, sinRoll, cosHeading, sinHeading; // trigonometric values of the Euler angles
extern PT1 magHeadingCorrection; // PT1 filter for the IMU heading correction (from yaw to heading via compass) (updated on every compass read)
extern fix32 magFilterCutoff; // filter frequency for the magnetometer data
extern const fix32 &vVel; // vertical velocity of the drone (up = positive, m/s)
extern const fix32 &combinedAltitude; // altitude of the drone (in meters ASL) by combining the barometer, GPS and the accelerometer
extern PT1 eVelFilter; // east velocity filter, in m/s, updated by GPS and interpolated using accelerometer data
extern PT1 nVelFilter; // north velocity filter, in m/s, updated by GPS and interpolated using accelerometer data
extern const fix32 &eVel; // east velocity of the drone (m/s) by GPS + accel (filtered)
extern const fix32 &nVel; // north velocity of the drone (m/s) by GPS + accel (filtered)
extern fix32 vAccel; // vertical up acceleration of the drone (m/s^2) provided by the accelerometer
extern Quaternion q; // attitude quaternion of the drone, used to calculate the attitude angles and the heading
extern PT1 baroImuUpVelFilter;
extern volatile u8 altInitState; // monotonically increasing, 0 = neither baro nor GPS had their first valid alt, 1 = initialized by baro, 2 = initialized by GPS

/**
 * @brief initialize the IMU
 * @details setting start values for quaternion, attitude angles and mag filter rollover
 */
void imuInit();

/**
 * @brief feeds gyro data into the attitude quaternion
 *
 * Called at 3200Hz
 */
void imuGyroUpdate();

/**
 * @brief Feeds accel data into the quaternion to prevent drift
 *
 * Called at 400Hz
 */
void imuAccelUpdate1();

/**
 * @brief Feeds accel data into the quaternion to prevent drift
 *
 * Called at 400Hz
 */
void imuAccelUpdate2();

/**
 * @brief updates roll, pitch and yaw values, as well as combined heading via the filtered data
 *
 * Called at 400Hz
 */
void imuUpdatePitchRoll();

/**
 * @brief updates altitude, vVel and horizontal velocities from accelerometer (and barometer) data
 *
 * Called at 400Hz
 */
void imuUpdateSpeeds();
