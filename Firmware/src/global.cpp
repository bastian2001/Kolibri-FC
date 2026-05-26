/**
 * @file global.cpp
 * @brief Global variables and helper functions
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

#include "global.h"

bool parseInt(const char *str, i64 &value) {
	if (!str) return false;

	errno = 0;
	char *end = nullptr;

	i64 v = std::strtoll(str, &end, 10);
	if (end == str) return false;
	if (*end != '\0') return false;
	if (errno == ERANGE && (value == LONG_LONG_MAX || value == LONG_LONG_MIN)) return false;

	value = v;
	return true;
}
bool parseFloat(const char *str, f64 &value) {
	if (!str) return false;

	errno = 0;
	char *end = nullptr;

	f64 v = std::strtod(str, &end);
	if (end == str) return false;
	if (*end != '\0') return false;
	if (errno == ERANGE && (value == HUGE_VAL || value == -HUGE_VAL || value == 0.0)) return false;

	value = v;
	return true;
}

std::optional<ExpressLRS> elrs;

NeoPixelConnect p(PIN_LEDS, 9, PIO_LED);
std::string uavName = "";

bool core1_separate_stack = true; // 8k+8k stack instead of 4k+4k
