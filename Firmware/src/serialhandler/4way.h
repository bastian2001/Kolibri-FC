/**
 * @file 4way.h
 * @brief Function declarations for 4-way interface
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
#include <Arduino.h>

/**
 * @brief start the 4way interface to the ESCs
 *
 * @param serialNum Serial number that should get the SERIAL_4WAY_HOST role
 */
void begin4Way(u8 serialNum);

void process4Way(u8 c);

void end4Way();
