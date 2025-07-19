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
#endif
