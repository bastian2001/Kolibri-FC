#pragma once

/// @brief Smoothes the sticks and applied rate cuve to them
void runAcroMode();

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
