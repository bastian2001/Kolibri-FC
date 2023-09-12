#include "global.h"

PIO escPio;
uint8_t escSm;

uint32_t motorPacket[2] = {0, 0xF000F}; // all motors off, but request telemetry in 12th bit

void initESCs()
{
	escPio = pio0;
	uint offset = pio_add_program(escPio, &dshotx4_program);
	escSm = pio_claim_unused_sm(escPio, true);
	pio_sm_config c = dshotx4_program_get_default_config(offset);
	Serial.println("Prepared PIO program");

	pio_gpio_init(escPio, PIN_MOTORS);
	pio_gpio_init(escPio, PIN_MOTORS + 1);
	pio_gpio_init(escPio, PIN_MOTORS + 2);
	pio_gpio_init(escPio, PIN_MOTORS + 3);
	sm_config_set_set_pins(&c, PIN_MOTORS, 4);
	sm_config_set_out_pins(&c, PIN_MOTORS, 4);
	sm_config_set_in_pins(&c, PIN_MOTORS);
	sm_config_set_jmp_pin(&c, PIN_MOTORS);
	Serial.println("Prepared PIO pins");

	sm_config_set_out_shift(&c, false, false, 32);
	Serial.println("prepared shift registers");

	pio_sm_set_consecutive_pindirs(escPio, escSm, PIN_MOTORS, 4, true);
	pio_sm_init(escPio, escSm, offset, &c);
	pio_sm_set_enabled(escPio, escSm, true);
	pio_sm_set_clkdiv_int_frac(escPio, escSm, dshotx4_CLKDIV_300_INT, dshotx4_CLKDIV_300_FRAC);
	pio_sm_put(escPio, escSm, 0);
	pio_sm_put(escPio, escSm, 0);
	Serial.println("Enabled PIO");
}

uint16_t appendChecksum(uint16_t data)
{
	int csum = data;
	csum ^= data >> 4;
	csum ^= data >> 8;
	// csum = ~csum;
	csum &= 0xF;
	return (data << 4) | csum;
}

void sendRaw16Bit(const uint16_t raw[4])
{
	motorPacket[0] = 0;
	motorPacket[1] = 0;
	for (int i = 31; i >= 0; i--)
	{
		int pos = i / 4;
		int motor = i % 4;
		motorPacket[0] |= ((raw[motor] >> (pos + 8)) & 1) << i;
		motorPacket[1] |= ((raw[motor] >> pos) & 1) << i;
	}
	pio_sm_put(escPio, escSm, motorPacket[0]);
	pio_sm_put(escPio, escSm, motorPacket[1]);
}

void sendRaw11Bit(const uint16_t raw[4])
{
	static uint16_t t[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++)
	{
		t[i] = constrain(raw[i], 0, 2047);
		t[i] = appendChecksum(t[i] << 1 | 1);
	}
	Serial.println(t[0]);
	sendRaw16Bit(t);
}

void sendThrottles(const int16_t throttles[4])
{
	static uint16_t t[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++)
	{
		t[i] = constrain(throttles[i], 0, 2000);
		if (t[i])
			t[i] += 47;
		t[i] = appendChecksum(t[i] << 1 | 0);
	}
	sendRaw16Bit(t);
}