
#include "Wire.h"
#include "global.h"
u8 i2c0blocker = 0; // 0: free, 1: blocked for baro, 2: for mag

static void startI2cTransmission(u8 addr, bool read) {
	i2c0->hw->enable = 0;
	i2c0->hw->tar = addr;
	i2c0->hw->enable = 1;

	while (i2c_get_read_available(i2c0))
		i2c0->hw->data_cmd; // flush RX FIFO
}

void startI2cRead(u8 addr, u8 len) {
	startI2cTransmission(addr, true);
	if (len > 16) len = 16;
	for (int i = 0; i < len; i++) {
		u32 last = i == len - 1;

		// clear any errors -> we do not have any way to deal with them appropriately
		i2c0->hw->tx_abrt_source;
		if (i2c0->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS) {
			i2c0->hw->clr_tx_abrt;
		}

		while (!i2c_get_write_available(i2c0))
			tight_loop_contents();

		i2c0->hw->data_cmd = (last << I2C_IC_DATA_CMD_STOP_LSB) | I2C_IC_DATA_CMD_CMD_BITS; // -> 1 for read
	}
}

void startI2cWrite(u8 addr, u8 *buf, u8 len) {
	startI2cTransmission(addr, false);
	if (len > 16) len = 16;
	for (int i = 0; i < len; i++) {
		u32 last = i == len - 1;

		// clear any errors -> we do not have any way to deal with them appropriately
		if (i2c0->hw->tx_abrt_source) {
			i2c0->hw->clr_tx_abrt;
		}

		while (!i2c_get_write_available(i2c0))
			tight_loop_contents();

		i2c0->hw->data_cmd = last << I2C_IC_DATA_CMD_STOP_LSB | *buf++;
	}
}

int checkI2cWriteDone() {
	if (i2c0->hw->tx_abrt_source) {
		i2c0->hw->clr_tx_abrt;
		return -1;
	}
	if (i2c0->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS) {
		return 1;
	}
	return 0;
}

int getI2cReadCount() {
	if (i2c0->hw->tx_abrt_source) {
		i2c0->hw->clr_tx_abrt;
		return -1;
	}
	return i2c_get_read_available(i2c0);
}

int getI2cReadData(u8 *buf, u8 len) {
	if (len > 16) len = 16;
	for (int i = 0; i < len; i++) {
		*buf++ = (u8)i2c0->hw->data_cmd;
	}
	return len;
}
