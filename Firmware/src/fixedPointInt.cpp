#include "global.h"
#include "hardware/interp.h"

fix32 sinLut[257];
interp_config sinInterpConfig0, sinInterpConfig1;
const fix32 FIX_PI   = PI;
const fix32 FIX_2PI  = 2 * PI;
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
 * accurate to about 0.0001. Important: Call initFixTrig() once at the start. Also call startFixTrig() once before every sinFix/cosFix calculation batch to prepare the interpolator for blend mode
 * @param x
 * @return fix32
 */
fix32 sinFix(const fix32 x) {
	i32 xNew = (x / FIX_PI).getRaw();
	i32 sign = ((xNew >> 16) & 1) * 2 - 1; // 1 if 0 <= x < PI +/-2n*PI, -1 otherwise
	xNew &= 0xFFFF;                        // %= PI
	u32 high          = xNew >> 8;
	u32 low           = xNew & 0xFF;
	i32 y0            = sinLut[high].getRaw();
	i32 y1            = sinLut[high + 1].getRaw();
	interp0->accum[1] = low;
	interp0->base[0]  = y0;
	interp0->base[1]  = y1;
	return fix32::fromRaw(interp0->peek[1] * sign);
}
/**
 * @brief calculates the cosine of a fixed point number, faster than cosf
 * accurate to about 0.0001. Important: Call initFixTrig() once at the start. Also call startFixTrig() once before every sinFix/cosFix calculation batch to prepare the interpolator for blend mode
 * @param x
 * @return fix32
 */
fix32 cosFix(const fix32 x) { return sinFix(x + FIX_PI_2); }

// ================================================ fix64 ================================================

fix64 fix64::fromRaw(i64 v) {
	fix64 result;
	return result.setRaw(v);
}
fix64::fix64(const i32 v) {
	this->value = (i64)v << 32;
}
fix64::fix64(const int v) {
	this->value = (i64)v << 32;
}
fix64::fix64(const f32 v) {
	this->value = (i64)(v * 4294967296);
}
fix64 fix64::setRaw(const i64 v) {
	this->value = v;
	return *this;
}
i64 fix64::getRaw() const {
	return this->value;
}
f32 fix64::getf32() const {
	return (f32)this->value / 4294967296;
}
f64 fix64::getf64() const {
	return (f64)this->value / 4294967296;
}
i32 fix64::getInt() const {
	return (i32)(this->value >> 32);
}
fix32 fix64::toFixed32() const {
	fix32 result;
	return result.setRaw((i32)(this->value >> 16));
}
fix64 fix64::operator+(const fix64 other) const {
	fix64 result;
	return result.setRaw(this->value + other.getRaw());
}
fix64 fix64::operator-(const fix64 other) const {
	return fix64::fromRaw(this->value - other.getRaw());
}
fix64 fix64::operator+(const fix32 other) const {
	fix64 result;
	return result.setRaw(this->value + (((i64)other.getRaw()) << 16));
}
fix64 fix64::operator-(const fix32 other) const {
	fix64 result;
	return result.setRaw(this->value - (((i64)other.getRaw()) << 16));
}
fix64 fix64::operator+=(const fix32 other) {
	this->value += ((i64)other.getRaw()) << 16;
	return *this;
}
fix64 fix64::operator*(const fix64 other) const {
	i64 raw1    = this->value;
	i64 raw2    = other.getRaw();
	i64 pos1    = raw1 >= 0 ? raw1 : -raw1;
	i64 pos2    = raw2 >= 0 ? raw2 : -raw2;
	u64 big     = (pos1 >> 32) * (pos2 >> 32);
	u64 small   = (pos1 & 0xFFFFFFFF) * (pos2 & 0xFFFFFFFF);
	u64 med     = (pos1 >> 32) * (pos2 & 0xFFFFFFFF) + (pos1 & 0xFFFFFFFF) * (pos2 >> 32);
	i64 result2 = med + ((small >> 32) & 0xFFFFFFFF) + (big << 32);
	if (raw1 < 0) result2 = -result2;
	if (raw2 < 0) result2 = -result2;
	return fix64::fromRaw(result2);
}
fix32 fix64::operator*(const fix32 other) const {
	fix32 result;
	return result.setRaw((i32)(((i64)this->value * (i64)other.getRaw()) >> 32));
}
fix64 fix64::operator*(const int other) const {
	fix64 result;
	return result.setRaw(this->value * other);
}
fix64 fix64::multiply64(const fix32 other) const {
	fix64 result;
	return result.setRaw((i64)(((i64)this->value * (i64)other.getRaw()) >> 16));
}
fix64 fix64::operator=(const i32 other) {
	this->value = (i64)other << 32;
	return *this;
}
fix64 fix64::operator=(const int other) {
	this->value = (i64)other << 32;
	return *this;
}
fix64 fix64::operator=(const i64 other) {
	this->value = other << 32;
	return *this;
}
fix64 fix64::operator=(const fix32 other) {
	this->value = ((i64)(other.getRaw())) << 16;
	return *this;
}
fix64 fix64::operator=(const f32 other) {
	this->value = (i64)(other * 4294967296);
	return *this;
}
fix64 fix64::operator=(const f64 other) {
	this->value = (i64)(other * 4294967296);
	return *this;
}
fix64 fix64::operator>>(const i32 other) const {
	return fix64::fromRaw(this->value >> other);
}
fix64 fix64::operator-() const {
	return fix64::fromRaw(-this->value);
}
