/*
 * Firmware for BlHeli-Passthrough
 * Prototype Version V1.0
 */
#include <Arduino.h>

#include "Global.h"      /* Global Variables*/
#include "serial_comm.h" /* Serial Code*/

void setup() {
	Serial.begin(115200);
}

void loop() {
	process_serial(); // Process Serial MSP/4-Way Data
}
