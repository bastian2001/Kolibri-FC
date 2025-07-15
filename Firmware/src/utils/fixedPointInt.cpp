#include "global.h"
#include "hardware/interp.h"

fix32 sinLut[257];
fix32 cosLut[257];
fix32 atanLut[257];
fix32 acosLut[257];
interp_config sinInterpConfig0, sinInterpConfig1;

void initFixTrig() {
	for (int i = 0; i <= 256; i++) {
		sinLut[i] = sin(i * PI / 256);
		cosLut[i] = cos(i * PI / 256);
		atanLut[i] = atan((f64)i / 256);
		acosLut[i] = FIX_PI / 2 - acos((f64)i / 256);
	}
	sinInterpConfig0 = interp_default_config();
	sinInterpConfig1 = interp_default_config();
	interp_config_set_blend(&sinInterpConfig0, 1);
	interp_config_set_signed(&sinInterpConfig1, 1);
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
	interp0->accum[1] = xRaw & 0xFF;
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
	interp0->accum[1] = x.raw & 0xFF;
	interp0->base[0] = acosLut[high].raw;
	interp0->base[1] = acosLut[high + 1].raw;
	return FIX_PI / 2 - fix32().setRaw((i32)interp0->peek[1] * sign);
}
