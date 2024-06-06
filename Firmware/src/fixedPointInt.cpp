#include "global.h"
#include "hardware/interp.h"

fix32 sinLut[257];
interp_config sinInterpConfig0, sinInterpConfig1;
const fix32 FIX_PI = PI;
const fix32 FIX_2PI = 2 * PI;
const fix32 FIX_PI_2 = PI / 2;

void initFixTrig() {
	for (int i = 0; i <= 256; i++) {
		sinLut[i] = sin(i * PI / 256);
	}
	sinInterpConfig0 = interp_default_config();
	sinInterpConfig1 = interp_default_config();
	interp_config_set_blend(&sinInterpConfig0, 1);
}

/**
 * @brief calculates the sine of a fixed point number, faster than sinf
 * @details accurate to about 0.0001. Important: Call initFixTrig() once at the start.
 * Also call startFixTrig() once before every sinFix/cosFix calculation batch to prepare the interpolator for blend mode
 * @param x
 * @return fix32
 */
fix32 sinFix(const fix32 x) {
	i32 xNew = (x / FIX_PI).raw;
	i32 sign = 1 - ((xNew >> 16) & 1) * 2; // 1 if 0 <= x < PI +/-2n*PI, -1 otherwise
	xNew &= 0xFFFF; // %= PI
	u32 high = xNew >> 8;
	u32 low = xNew & 0xFF;
	i32 y0 = sinLut[high].raw;
	i32 y1 = sinLut[high + 1].raw;
	interp0->accum[1] = low;
	interp0->base[0] = y0;
	interp0->base[1] = y1;
	return fix32().setRaw(interp0->peek[1] * sign);
}
/**
 * @brief calculates the cosine of a fixed point number, faster than cosf
 * @details accurate to about 0.0001. Important: Call initFixTrig() once at the start. Also call startFixTrig() once before every sinFix/cosFix calculation batch to prepare the interpolator for blend mode
 * @param x
 * @return fix32
 */
fix32 cosFix(const fix32 x) { return sinFix(x + FIX_PI_2); }
