/**
 * @file i2c.h
 * @brief Functions for asynchronous I2C communication
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
#include "typedefs.h"
extern u8 i2c0blocker;

void startI2cRead(u8 addr, u8 len);

void startI2cWrite(u8 addr, u8 *buf, u8 len);

int checkI2cWriteDone();

int getI2cReadCount();

int getI2cReadData(u8 *buf, u8 len);
