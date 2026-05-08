/**
 * @file customSimdMath.h
 * @brief Functions to speed up some math operations using ARM SIMD instructions. Only used on Cortex-M4 and M7 targets.
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

#include "arm_acle.h"

#if __ARM_FEATURE_SIMD32
static inline int16x2_t max16x2(int16x2_t x, int16x2_t y) {
	__ssub16(x, y);
	return __sel(x, y);
}
static inline int16x2_t min16x2(int16x2_t x, int16x2_t y) {
	__ssub16(x, y);
	return __sel(y, x);
}
/**
 * @brief selects the min of the two high hw and the max of the two low hw
 *
 * @param x int16x2_t first operand
 * @param y int16x2_t second operand
 * @return int16x2_t min(x hw, y hw) << 16 | max(x lw, y lw)
 */
static inline int16x2_t minmax16x2(int16x2_t x, int16x2_t y) {
	int16x2_t temp = __ssub16(x, y); // xhw - yhw, xlw - ylw
	temp = __ror(temp, 16); // xlw - ylw, xhw - yhw
	__sasx(0, temp); // xhw - yhw, -(xlw - ylw)
	return __sel(y, x); // yhw if xhw >= yhw, ylw if ylw >= xlw
}
#endif
