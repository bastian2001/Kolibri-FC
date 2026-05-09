/**
 * @file control.h
 * @brief Interface declaration for control functions => generate setpoints for the PID controller based on the flight mode and the pilot's stick input
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

#pragma once

enum class FlightMode {
	ACRO,
	ANGLE,
	ALT_HOLD,
	GPS, // control the velocity of the drone
	GPS_WP, // waypoint mode, i.e. GPS mode with automatic control, for now it just goes to the homepoint and hovers there
	LENGTH // place behind all other modes, acts as a limit for loops etc.
};
extern volatile FlightMode flightMode; // currently selected flight mode (NOT whether the drone is armed)

enum {
	ACTUAL_CENTER_SENSITIVITY, // center sensitivity for ACTUAL rates => linear part, in deg/s
	ACTUAL_MAX_RATE, // maximum rate for ACTUAL rates => including exponential part, in deg/s
	ACTUAL_EXPO // expo factor for ACTUAL rates, between 0 and 1
};

extern fix32 hvelFfFilterCutoff; // cutoff frequency for the horizontal velocity feedforward filter (Hz)
extern fix32 hvelIRelaxFilterCutoff; // cutoff frequency for the horizontal velocity I term relax filter (Hz)
extern fix32 hvelPushFilterCutoff; // cutoff frequency for the horizontal velocity push filter (Hz)
extern fix32 vvelDFilterCutoff; // cutoff frequency for the vertical velocity D filter (Hz)
extern fix32 vvelFFFilterCutoff; // cutoff frequency for the vertical velocity feedforward filter (Hz)
extern fix32 vVelSetpoint;

/**
 * @brief calculates the distance in meters between two GPS coordinates
 *
 * Takes a trivial approach, which is not very accurate for large distances or when close to the poles.
 * It finds the shortest path on an equirectangular projection of the Earth.
 *
 * @attention requires startFixMath to be called beforehand
 *
 * @param lat1 latitude of first coordinate in deg
 * @param lon1 longitude of first coordinate in deg
 * @param lat2 latitude of second coordinate in deg
 * @param lon2 longitude of second coordinate in deg
 * @param distNorth distance to go north from first to second coordinate
 * @param distEast distance to go east from first to second coordinate
 */
void distFromCoordinates(fix64 lat1, fix64 lon1, fix64 lat2, fix64 lon2, fix32 *distN, fix32 *distE);

/// @brief set and initialize a flight mode
/// the flight mode is applied without plausibility checks
void setFlightMode(FlightMode mode);

/**
 * @brief Sets the rotational setpoints for the quad movement
 *
 * Depending on the mode, the setpoints are directly calculated from the sticks (acro) or via attitude (angle) or even by position (GPS)
 *
 * If runnning in acro mode, the effective rate is the PID rate. If running in angle mode or above, the control rate is 400Hz.
 */
void controlLoop();

void controlDisarmedLoop();

void initControl();
