/**
 * @file baro.h
 * @brief Barometer driver interface (currently SPL006 or LPS22, depending on the target)
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <Arduino.h>
#include <fixedPointInt.h>

extern fix32 baroASL;
extern fix32 baroUpVel;
extern f32 baroPres;
extern u8 baroTemp;
extern fix32 gpsBaroAlt;
extern i32 pressureRaw;
extern volatile i32 blackboxPres;

#if HW_BARO == BARO_LPS22
#define I2C_BARO_ADDR 0x5D
enum BaroRegs {
	INTERRUPT_CFG = 0x0B,
	THS_P_L = 0x0C,
	THS_P_H = 0x0D,
	WHO_AM_I = 0x0F, // read-only
	CTRL_REG1 = 0x10,
	CTRL_REG2 = 0x11,
	CTRL_REG3 = 0x12,
	FIFO_CTRL = 0x14,
	REF_P_XL = 0x15,
	REF_P_L = 0x16,
	REF_P_H = 0x17,
	RPDS_L = 0x18,
	RPDS_H = 0x19,
	RES_CONF = 0x1A,
	INT_SOURCE = 0x25, // read-only
	FIFO_STATUS = 0x26, // read-only
	STATUS = 0x27, // read-only
	PRESS_OUT_XL = 0x28, // read-only
	PRESS_OUT_L = 0x29, // read-only
	PRESS_OUT_H = 0x2A, // read-only
	TEMP_OUT_L = 0x2B, // read-only
	TEMP_OUT_H = 0x2C, // read-only
	LPFP_RES = 0x33, // read-only
};
#endif

/**
 * @brief The main barometer loop function
 *
 * @details This function is called periodically to check for new, read and process the barometer data as well as initializing it. It calculates the altitude above sea level, the vertical velocity and the pressure.
 */
void baroLoop();
