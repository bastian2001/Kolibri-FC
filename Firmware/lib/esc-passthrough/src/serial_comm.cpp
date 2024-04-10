#include <Arduino.h>

#include "../../../src/pins.h"
#include "4Way.h"        // 4way process
#include "Global.h"      // globale variablen
#include "MSP.h"         // MSP process
#include "serial_comm.h" // Header

uint16_t serial_rx_counter = 0;
uint16_t serial_tx_counter = 0;
uint16_t serial_buffer_len = 0;

bool serial_command = false;

void delayWhileRead(u16 ms);

void process_serial(void) {
	Serial2.print(1);
	if (Serial.available()) {
		delayWhileRead(10);
		serial_command = true;
		while (Serial.available()) {
			serial_rx[serial_rx_counter] = Serial.read();

			if ((serial_rx_counter == 4) && (serial_rx[0] == 0x2F)) {
				// 4 Way Command: Size (0 = 256) + 7 Byte Overhead
				if (serial_rx[4] == 0) {
					serial_buffer_len = 256 + 7;
				} else {
					serial_buffer_len = serial_rx[4] + 7;
				}
			}
			if ((serial_rx_counter == 3) && (serial_rx[0] == 0x24)) {
				// MSP Command: Size (0 = 0)+ 6 Byte Overhead
				serial_buffer_len = serial_rx[3] + 6;
			}

			serial_rx_counter++;
			if (serial_rx_counter == serial_buffer_len) {
				break;
			}
		}
	}
	Serial2.print(2);
	if (serial_command) {
		if ((serial_rx[0] == 0x2F) && (serial_rx_counter == serial_buffer_len)) {
			// 4 Way Command
			// 4 Way proceed
			Serial2.print(3);
			serial_tx_counter = Check_4Way(serial_rx);
			Serial2.printf("TX4: %d\n", serial_tx_counter);
			for (uint16_t b = 0; b < serial_tx_counter; b++) {
				Serial.write(serial_rx[b]);
			}
			Serial2.print("done");
			serial_command    = false;
			serial_rx_counter = 0;
			serial_tx_counter = 0;
		} else if (serial_rx[0] == 0x24 && serial_rx[1] == 0x4D && serial_rx[2] == 0x3C) {
			// MSP Command
			// MSP Proceed
			Serial2.print(4);
			serial_tx_counter = MSP_Check(serial_rx, serial_rx_counter);
			Serial2.printf("TXM: %d\n", serial_tx_counter);
			for (uint16_t b = 0; b < serial_tx_counter; b++) {
				Serial.write(serial_rx[b]);
			}
			Serial2.print("done");
			serial_command    = false;
			serial_rx_counter = 0;
			serial_tx_counter = 0;
		}
	}
}
