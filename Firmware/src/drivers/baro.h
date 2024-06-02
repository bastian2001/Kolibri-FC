#include "fixedPointInt.h"
#include <Arduino.h>

extern f32 baroASL;
extern f32 baroUpVel;
extern f32 baroPres;
extern u8 baroTemp;
extern fix32 gpsBaroAlt;

/// Initializes the barometer (Goertek SPL06-007)
void initBaro();

/// Checks if a new baro value is available and reads it
void readBaroLoop();

/**
 * @brief Processes the previously read baro value
 *
 * @details This function is called periodically to process the barometer data. It calculates the altitude above sea level, the vertical velocity and the pressure.
 *
 * This function can be called on another core than readBaroLoop() to split the workload.
 */
void evalBaroLoop();