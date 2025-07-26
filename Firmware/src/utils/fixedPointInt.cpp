#include "global.h"
#include "hardware/interp.h"

fix32 sinLut[257];
fix32 cosLut[257];
fix32 atanLut[257];
fix32 acosLut[257];
fix32 sqrtLut[769];

interp_config fixMathInterpConfig0, fixMathInterpConfig1;

void initFixMath() {
	for (int i = 0; i <= 256; i++) {
		sinLut[i] = sin(i * PI / 256);
		cosLut[i] = cos(i * PI / 256);
		atanLut[i] = atan((f64)i / 256);
		acosLut[i] = FIX_PI / 2 - acos((f64)i / 256);
	}
	for (int i = 0; i <= 768; i++) {
		sqrtLut[i] = sqrt((i + 256) / 256.);
	}
	fixMathInterpConfig0 = interp_default_config();
	fixMathInterpConfig1 = interp_default_config();
	interp_config_set_blend(&fixMathInterpConfig0, 1);
	interp_config_set_signed(&fixMathInterpConfig1, 1);
}

fix32 sinFix(const fix32 x) {
	i32 xNew = (x / FIX_PI).raw;
	i32 sign = 1 - ((xNew >> 16) & 1) * 2; // 1 if 0 <= x < PI +/-2n*PI, -1 otherwise
	xNew &= 0xFFFF; // %= PI
	u32 high = xNew >> 8;
	interp0->accum[1] = xNew;
	interp0->base[0] = sinLut[high].raw;
	interp0->base[1] = sinLut[high + 1].raw;
	return fix32().setRaw(interp0->peek[1] * sign);
}
fix32 cosFix(const fix32 x) {
	i32 xNew = (x / FIX_PI).raw;
	i32 sign = 1 - ((xNew >> 16) & 1) * 2; // 1 if 0 <= x < PI +/-2n*PI, -1 otherwise
	xNew &= 0xFFFF; // %= PI
	u32 high = xNew >> 8;
	interp0->accum[1] = xNew;
	interp0->base[0] = cosLut[high].raw;
	interp0->base[1] = cosLut[high + 1].raw;
	return fix32().setRaw(interp0->peek[1] * sign);
}

void sinCosFix(const fix32 x, fix32 &sinOut, fix32 &cosOut) {
	i32 xNew = (x / FIX_PI).raw;
	i32 sign = 1 - ((xNew >> 16) & 1) * 2; // 1 if 0 <= x < PI +/-2n*PI, -1 otherwise
	xNew &= 0xFFFF; // %= PI
	u32 high = xNew >> 8;
	interp0->accum[1] = xNew;
	interp0->base[0] = sinLut[high].raw;
	interp0->base[1] = sinLut[high + 1].raw;
	sinOut.setRaw(interp0->peek[1] * sign);
	interp0->base[0] = cosLut[high].raw;
	interp0->base[1] = cosLut[high + 1].raw;
	cosOut.setRaw(interp0->peek[1] * sign);
}

fix32 atanFix(fix32 x) {
	i32 sign = x.sign();
	i32 offset = 0;
	x *= sign;
	if (x > fix32(1)) {
		x = fix32(1) / x;
		offset = (FIX_PI >> 1).raw * sign;
		sign = -sign;
	} else if (x == 1) {
		return fix32().setRaw((FIX_PI / 4).raw * sign);
	}
	i32 &xRaw = x.raw;
	u32 high = xRaw >> 8;
	interp0->accum[1] = xRaw;
	interp0->base[0] = atanLut[high].raw;
	interp0->base[1] = atanLut[high + 1].raw;
	return fix32().setRaw((i32)interp0->peek[1] * sign + offset);
}
fix32 atan2Fix(const fix32 y, const fix32 x) {
	if (x != 0)
		return atanFix(y / x) + FIX_PI * (x.raw < 0) * y.sign();
	return FIX_PI / 2 * y.sign();
}

fix32 acosFix(fix32 x) {
	i32 sign = x.sign();
	x *= sign;
	if (x >= fix32(1)) return FIX_PI / 2 - FIX_PI / 2 * sign; // if abs >= 1, assume the value was +-1
	u32 high = x.raw >> 8;
	interp0->accum[1] = x.raw;
	interp0->base[0] = acosLut[high].raw;
	interp0->base[1] = acosLut[high + 1].raw;
	return FIX_PI / 2 - fix32().setRaw((i32)interp0->peek[1] * sign);
}

/**
 * @brief calculates the square root of any number between 1 and 4. x is not checked for this range! potential panic if out of range
 *
 * Uses the interpolator with 768 steps for this range. Output resolution of 17 bits
 *
 * @param x the input for the root function, between 1 and 4
 * @return fix32 result of the root, therefore between 1 and 2
 */
static fix32 sqrtFix_1_4(fix32 x) {
	u32 high = (x.raw >> 8) - 256;
	interp0->accum[1] = x.raw;
	interp0->base[0] = sqrtLut[high].raw;
	interp0->base[1] = sqrtLut[high + 1].raw;
	return fix32().setRaw((i32)interp0->peek[1]);
}

fix32 sqrtFix(fix32 x) {
	if (x <= 0) return 0;
#if __ARM_FEATURE_CLZ
	// how many divisions/multiplications by 4 we can make to get x to the 1..4 range
	// "14 - " gives us the shifts we need (right positive, left negative)
	i32 shift = 14 - (__clz(x.raw) >> 1 << 1);
#elif
#error "No CLZ support"
#endif
	if (shift >= 0) {
		x = sqrtFix_1_4(x >> shift);
		return x << (shift >> 1);
	}
	shift = -shift;
	x = sqrtFix_1_4(x << shift);
	return x >> (shift >> 1);
}
