/**
 * @file adc.h
 * @brief Header file for ADC reading and battery management implementation
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

#include "typedefs.h"
extern u16 adcVoltage;
extern f32 adcCurrent;

extern u8 batCells;
extern u8 batState; // 0 = no battery (USB or startup), 1 = battery found
extern u8 cellCountSetting;
extern u16 emptyVoltageSetting;
extern u32 adcFlag; // high halfword for current, low halfword for voltage measurements, set to 0xFFFF when new value available

/// @brief enables ADC pins for voltage and current monitoring, enables OSD elements
void initADC();

/// @brief reads the voltage, temperature and current from the ADC
void adcLoop();
