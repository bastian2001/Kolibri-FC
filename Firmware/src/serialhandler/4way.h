#pragma once
#include "typedefs.h"
#include <Arduino.h>

/**
 * @brief start the 4way interface to the ESCs
 *
 * @param serial Serial that should get the SERIAL_4WAY_HOST role
 */
void begin4Way(KoliSerial *serial);

void process4Way(u8 c);

void end4Way();
