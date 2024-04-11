#include <Arduino.h>

#include "4Way.h" // 4way process
#include "ESC_Serial.h"
#include "Global.h"          // globale variablen
#include "MSP.h"             // MSP process
#include "esc_passthrough.h" // Header
#include "hardware/pio.h"
#include "onewire_receive.pio.h"
#include "onewire_transmit.pio.h"

uint16_t serial_rx_counter = 0;
uint16_t serial_tx_counter = 0;
uint16_t serial_buffer_len = 0;

bool serial_command = false;

bool passthroughSetup = false;

uint8_t passthroughPin = 255;

gpio_function externalPinFunction = GPIO_FUNC_NULL;

void beginPassthrough(uint8_t pin, PIO pio, int8_t sm) {
	if (passthroughSetup) {
		return;
	}
	escPassthroughPio = pio;
	offsetPioReceive  = pio_add_program(escPassthroughPio, &onewire_receive_program);
	offsetPioTransmit = pio_add_program(escPassthroughPio, &onewire_transmit_program);
	if (sm == -1) {
		escPassthroughSm = pio_claim_unused_sm(escPassthroughPio, true);
	} else {
		escPassthroughSm = sm;
		pio_claim_sm_mask(escPassthroughPio, 1u << sm);
	}
	externalPinFunction = gpio_get_function(pin);
	pio_gpio_init(escPassthroughPio, pin);
	gpio_set_pulls(pin, true, false);
	configPioReceive = onewire_receive_program_get_default_config(offsetPioReceive);
	sm_config_set_set_pins(&configPioReceive, pin, 1);
	sm_config_set_in_pins(&configPioReceive, pin);
	sm_config_set_jmp_pin(&configPioReceive, pin);
	sm_config_set_in_shift(&configPioReceive, true, false, 32);
	sm_config_set_clkdiv_int_frac(&configPioReceive, 859, 128);
	configPioTransmit = onewire_transmit_program_get_default_config(offsetPioTransmit);
	sm_config_set_set_pins(&configPioTransmit, pin, 1);
	sm_config_set_out_pins(&configPioTransmit, pin, 1);
	sm_config_set_out_shift(&configPioTransmit, true, false, 8);
	sm_config_set_clkdiv_int_frac(&configPioTransmit, 859, 128);
	pio_sm_init(escPassthroughPio, escPassthroughSm, offsetPioReceive, &configPioReceive);
	pio_sm_set_enabled(escPassthroughPio, escPassthroughSm, true);
	passthroughPin      = pin;
	passthroughBreakout = false;
	passthroughSetup    = true;
}

void endPassthrough() {
	if (!passthroughSetup) {
		return;
	}
	pio_sm_set_enabled(escPassthroughPio, escPassthroughSm, false);
	pio_sm_unclaim(escPassthroughPio, escPassthroughSm);
	gpio_set_function(passthroughPin, externalPinFunction);
	pio_remove_program(escPassthroughPio, &onewire_receive_program, offsetPioReceive);
	pio_remove_program(escPassthroughPio, &onewire_transmit_program, offsetPioTransmit);
	passthroughSetup = false;
}

uint8_t processPassthrough(void) {
	if (Serial.available()) {
		delay(10);
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
	if (serial_command) {
		if ((serial_rx[0] == 0x2F) && (serial_rx_counter == serial_buffer_len)) {
			// 4 Way Command
			// 4 Way proceed
			serial_tx_counter = Check_4Way(serial_rx);
			for (uint16_t b = 0; b < serial_tx_counter; b++) {
				Serial.write(serial_rx[b]);
			}
			serial_command    = false;
			serial_rx_counter = 0;
			serial_tx_counter = 0;
		} else if (serial_rx[0] == 0x24 && serial_rx[1] == 0x4D && serial_rx[2] == 0x3C) {
			// MSP Command
			// MSP Proceed
			serial_tx_counter = MSP_Check(serial_rx, serial_rx_counter);
			for (uint16_t b = 0; b < serial_tx_counter; b++) {
				Serial.write(serial_rx[b]);
			}
			serial_command    = false;
			serial_rx_counter = 0;
			serial_tx_counter = 0;
		}
	}
	return passthroughBreakout;
}
