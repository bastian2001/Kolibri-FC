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
