#pragma once
#include "typedefs.h"
#include <Arduino.h>

/**
 * @brief start the 4way interface to the ESCs
 *
 * @param serialNum Serial number that should get the SERIAL_4WAY role
 */
void begin4Way(u8 serialNum);

void process4Way(u8 c);

void end4Way();
