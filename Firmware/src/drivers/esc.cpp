#include "global.h"

u32 enableDShot = 1;
u32 escPioOffset = 0;

volatile u32 escRpm[4] = {0};
u8 escErpmFail = 0;

#define iv 0xFFFFFFFF
const u32 escDecodeLut[32] = {
	iv, iv, iv, iv, iv, iv, iv, iv, iv, 9, 10, 11, iv, 13, 14, 15,
	iv, iv, 2, 3, iv, 5, 6, 7, iv, 0, 8, 1, iv, 4, 12, iv};

void initESCs() {
	escPioOffset = pio_add_program(ESC_PIO, &bidir_dshot_x1_program);
	pio_claim_sm_mask(ESC_PIO, 0b1111);
	for (i32 i = 0; i < 4; i++) {
		pio_gpio_init(ESC_PIO, PIN_MOTORS + i);
		gpio_set_pulls(PIN_MOTORS + i, true, false);
		pio_sm_config c = bidir_dshot_x1_program_get_default_config(escPioOffset);
		sm_config_set_set_pins(&c, PIN_MOTORS + i, 1);
		sm_config_set_out_pins(&c, PIN_MOTORS + i, 1);
		sm_config_set_in_pins(&c, PIN_MOTORS + i);
		sm_config_set_jmp_pin(&c, PIN_MOTORS + i);
		sm_config_set_out_shift(&c, false, false, 32);
		sm_config_set_in_shift(&c, false, false, 32);
		pio_sm_init(ESC_PIO, i, escPioOffset, &c);
		pio_sm_set_consecutive_pindirs(ESC_PIO, i, PIN_MOTORS + i, 1, true);
		pio_sm_set_enabled(ESC_PIO, i, true);
		pio_sm_set_clkdiv_int_frac(ESC_PIO, i, bidir_dshot_x1_CLKDIV_600_INT, bidir_dshot_x1_CLKDIV_600_FRAC);
	}
}

u16 appendChecksum(u16 data) {
	int csum = data;
	csum ^= data >> 4;
	csum ^= data >> 8;
	csum = ~csum;
	csum &= 0xF;
	return (data << 4) | csum;
}

void sendRaw16Bit(const u16 raw[4]) {
	if (!enableDShot) return;
	for (i32 i = 0; i < 4; i++) {
		if (pio_sm_get_pc(ESC_PIO, i) != escPioOffset + 2)
			pio_sm_exec(ESC_PIO, i, pio_encode_jmp(escPioOffset + 1));
		pio_sm_put(ESC_PIO, i, ~(raw[i]));
	}
}

void sendRaw11Bit(const u16 raw[4]) {
	static u16 t[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++) {
		t[i] = constrain(raw[i], 0, 2047);
		t[i] = appendChecksum(t[i] << 1 | 1);
	}
	sendRaw16Bit(t);
}

void sendThrottles(const i16 throttles[4]) {
	static u16 t[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++) {
		t[i] = constrain(throttles[i], 0, 2000);
		if (t[i])
			t[i] += 47;
		t[i] = appendChecksum(t[i] << 1 | 0);
	}
	sendRaw16Bit(t);
}

void decodeErpm() {
	if (!enableDShot) return;
	tasks[TASK_ESC_RPM].runCounter++;
	elapsedMicros taskTimer = 0;
	for (int m = 0; m < 4; m++) {
		if (pio_sm_is_rx_fifo_empty(ESC_PIO, m)) {
			escErpmFail |= 1 << m;
			tasks[TASK_ESC_RPM].errorCount++;
			tasks[TASK_ESC_RPM].lastError = 1;
			condensedRpm[m] = 0;
			continue;
		}
		u32 edgeDetectedReturn = pio_sm_get_blocking(ESC_PIO, m);
		while (!pio_sm_is_rx_fifo_empty(ESC_PIO, m)) {
			edgeDetectedReturn = pio_sm_get_blocking(ESC_PIO, m);
		}
		edgeDetectedReturn = edgeDetectedReturn ^ (edgeDetectedReturn >> 1);
		u32 rpm = escDecodeLut[edgeDetectedReturn & 0x1F];
		rpm |= escDecodeLut[(edgeDetectedReturn >> 5) & 0x1F] << 4;
		rpm |= escDecodeLut[(edgeDetectedReturn >> 10) & 0x1F] << 8;
		rpm |= escDecodeLut[(edgeDetectedReturn >> 15) & 0x1F] << 12;
		u32 csum = (rpm >> 8) ^ rpm;
		csum ^= csum >> 4;
		csum &= 0xF;
		if (csum != 0x0F || rpm > 0xFFFF) {
			escErpmFail |= 1 << m;
			tasks[TASK_ESC_RPM].errorCount++;
			tasks[TASK_ESC_RPM].lastError = 2;
			condensedRpm[m] = 0;
			continue;
		}
		rpm >>= 4;
		condensedRpm[m] = rpm;
		if (rpm == 0xFFF) {
			escRpm[m] = 0;
		} else {
			rpm = (rpm & 0x1FF) << (rpm >> 9); // eeem mmmm mmmm
			if (!rpm) {
				escErpmFail |= 1 << m;
				continue;
			}
			rpm = (60000000 + 50 * rpm) / rpm;
			escRpm[m] = rpm / (MOTOR_POLES / 2);
			escErpmFail &= ~(1 << m);
		}
	}
	u32 duration = taskTimer;
	tasks[TASK_ESC_RPM].totalDuration += duration;
	if (duration > tasks[TASK_ESC_RPM].maxDuration)
		tasks[TASK_ESC_RPM].maxDuration = duration;
	if (duration < tasks[TASK_ESC_RPM].minDuration)
		tasks[TASK_ESC_RPM].minDuration = duration;
}