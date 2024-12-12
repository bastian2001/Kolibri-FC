#include "hardware/interp.h"
#include "typedefs.h"
#include <Arduino.h>
#pragma once

class fix32;
extern interp_config sinInterpConfig0, sinInterpConfig1;

extern const fix32 FIX_PI;
extern const fix32 FIX_2PI;
extern const fix32 FIX_PI_2;
extern const fix32 FIX_RAD_TO_DEG;
extern const fix32 FIX_DEG_TO_RAD;

void initFixTrig();
/**
 * @brief prepares the interpolator for blend mode
 * @details Call this once before every sinFix/cosFix calculation batch
 */
inline void startFixTrig() {
	interp_set_config(interp0, 0, &sinInterpConfig0);
	interp_set_config(interp0, 1, &sinInterpConfig1);
}

fix32 sinFix(const fix32 x);
fix32 cosFix(const fix32 x);
fix32 atanFix(const fix32 x);
fix32 atan2Fix(const fix32 y, const fix32 x);

class fix64 {
	// 32.32 fixed point
public:
	i64 raw = 0;
	inline constexpr fix64(){};

	// ======================== Type conversion from another type ========================
	inline constexpr fix64(const i32 v) {
		this->raw = (i64)v << 32;
	};
	inline constexpr fix64(const f32 v) {
		this->raw = (i64)(v * 4294967296);
	};
	inline constexpr fix64(const f64 v) {
		this->raw = (i64)(v * 4294967296);
	};
	inline constexpr fix64(const fix32 v);
	inline constexpr fix64 setRaw(const i64 raw) {
		this->raw = raw;
		return *this;
	};
	inline constexpr f32 getf32() const {
		return (f32)this->raw / 4294967296;
	};
	inline constexpr f64 getf64() const {
		return (f64)this->raw / 4294967296;
	};
	inline constexpr i32 geti32() const {
		return (i32)(this->raw >> 32);
	};
	inline constexpr u32 getu32() const {
		return (u32)(this->raw >> 32);
	};
	inline constexpr fix32 getfix32() const;

	// ======================== fix64 arithmetics ========================
	inline constexpr fix64 operator+(const fix64 other) const {
		return fix64().setRaw(this->raw + other.raw);
	};
	inline constexpr fix64 operator-(const fix64 other) const {
		return fix64().setRaw(this->raw - other.raw);
	};
	inline constexpr fix64 operator*(const fix64 other) const {
		i32 sign0 = this->sign();
		i32 sign1 = other.sign();
		i32 sign = sign0 * sign1;
		u64 pos0 = this->raw * sign0;
		u64 pos1 = other.raw * sign1;
		u64 big = (pos0 >> 32) * (pos1 >> 32);
		u64 small = (pos0 & 0xFFFFFFFF) * (pos1 & 0xFFFFFFFF);
		u64 med = (pos0 >> 32) * (pos1 & 0xFFFFFFFF) + (pos0 & 0xFFFFFFFF) * (pos1 >> 32);
		return fix64().setRaw((i64)((big << 32) + med + (small >> 32)) * sign);
	};
	inline constexpr bool operator==(const fix64 other) const {
		return this->raw == other.raw;
	};
	inline constexpr bool operator!=(const fix64 other) const {
		return this->raw != other.raw;
	};

	// ======================== fix32 arithmetics ========================
	inline constexpr fix64 operator+(const fix32 other) const;
	inline constexpr fix64 operator-(const fix32 other) const;
	inline constexpr fix64 operator*(const fix32 other) const;
	inline constexpr fix64 operator/(const fix32 other) const;

	// ======================== i32 arithmetics ========================
	inline constexpr fix64 operator+(const i32 other) const {
		return fix64().setRaw(this->raw + ((i64)other << 32));
	};
	inline constexpr fix64 operator-(const i32 other) const {
		return fix64().setRaw(this->raw - ((i64)other << 32));
	};
	inline constexpr fix64 operator*(const i32 other) const {
		return fix64().setRaw(this->raw * other);
	};
	inline constexpr fix64 operator/(const i32 other) const {
		return fix64().setRaw(this->raw / other);
	};

	// ======================== f32 arithmetics ========================
	inline constexpr fix64 operator+(const f32 other) const {
		return fix64().setRaw(this->raw + (i64)(other * 4294967296));
	};
	inline constexpr fix64 operator-(const f32 other) const {
		return fix64().setRaw(this->raw - (i64)(other * 4294967296));
	};
	inline constexpr fix64 operator*(const f32 other) const {
		return *this * (fix64)other;
	};
	inline constexpr fix64 operator/(const f32 other) const;

	inline constexpr fix64 operator>>(const i32 other) const {
		return fix64().setRaw(this->raw >> other);
	};
	inline constexpr fix64 operator<<(const i32 other) const {
		return fix64().setRaw(this->raw << other);
	};
	inline constexpr fix64 operator-() const {
		return fix64().setRaw(-this->raw);
	};
	inline constexpr bool operator<(const fix64 other) const {
		return this->raw < other.raw;
	}
	inline constexpr bool operator>(const fix64 other) const {
		return this->raw > other.raw;
	}
	inline constexpr i32 sign() const {
		return (this->raw >> 63) * 2 + 1;
	}
	inline constexpr fix64 abs() const {
		return *this * sign();
	}
};

class fix32 {
	// 16.16 fixed point
public:
	i32 raw = 0;
	inline constexpr fix32(){};

	// ======================== Type conversion from another type ========================
	inline constexpr fix32(const i32 v) {
		this->raw = v << 16;
	};
	inline constexpr fix32(const u32 v) {
		this->raw = v << 16;
	};
	inline constexpr fix32(const f32 v) {
		this->raw = (i32)(v * 65536);
	};
	inline constexpr fix32(const f64 v) {
		this->raw = (i32)(v * 65536);
	};
	inline constexpr fix32(const fix64 v) {
		this->raw = (i32)(v.raw >> 16);
	};

	// ======================== Raw handling ========================
	inline constexpr fix32 setRaw(const i32 v) {
		this->raw = v;
		return *this;
	};

	// ======================== Type conversion to another type ========================
	inline constexpr f32 getf32() const {
		return (f32)this->raw / 65536;
	};
	inline constexpr f64 getf64() const {
		return (f64)this->raw / 65536;
	};
	inline constexpr i32 geti32() const {
		return (i32)(this->raw >> 16);
	};
	inline constexpr u32 getu32() const {
		return (u32)(this->raw >> 16);
	};
	inline constexpr fix64 getfix64() const {
		return fix64().setRaw(((i64)this->raw) << 16);
	};

	// ======================== fix32 arithmetics ========================
	inline constexpr fix32 operator+(const fix32 other) const {
		return fix32().setRaw(this->raw + other.raw);
	};
	inline constexpr fix32 operator-(const fix32 other) const {
		return fix32().setRaw(this->raw - other.raw);
	};
	inline constexpr fix32 operator*(const fix32 other) const {
		return fix32().setRaw((i32)(((i64)this->raw * (i64)other.raw) >> 16));
	};
	inline constexpr fix32 operator/(const fix32 other) const {
		return fix32().setRaw((i32)((((i64)this->raw) << 16) / (i64)other.raw));
	};
	inline constexpr fix32 operator%(const fix32 other) const {
		return fix32().setRaw(this->raw % other.raw);
	};
	inline constexpr fix32 operator+=(const fix32 other) {
		this->raw += other.raw;
		return *this;
	};
	inline constexpr fix32 operator-=(const fix32 other) {
		this->raw -= other.raw;
		return *this;
	};
	inline constexpr fix32 operator*=(const fix32 other) {
		this->raw = (i32)(((i64)this->raw * (i64)other.raw) >> 16);
		return *this;
	};
	inline constexpr fix32 operator/=(const fix32 other) {
		this->raw = (i32)(((i64)this->raw << 16) / (i64)other.raw);
		return *this;
	};
	inline constexpr fix32 operator%=(const fix32 other) {
		this->raw %= other.raw;
		return *this;
	};
	inline constexpr bool operator==(const fix32 other) const {
		return this->raw == other.raw;
	};
	inline constexpr bool operator!=(const fix32 other) const {
		return this->raw != other.raw;
	};
	inline constexpr bool operator<(const fix32 other) const {
		return this->raw < other.raw;
	};
	inline constexpr bool operator>(const fix32 other) const {
		return this->raw > other.raw;
	};
	inline constexpr bool operator<=(const fix32 other) const {
		return this->raw <= other.raw;
	};
	inline constexpr bool operator>=(const fix32 other) const {
		return this->raw >= other.raw;
	};

	// ======================== i32 arithmetics ========================
	inline constexpr fix32 operator+(const i32 other) const {
		return fix32().setRaw(this->raw + (other << 16));
	};
	inline constexpr fix32 operator-(const i32 other) const {
		return fix32().setRaw(this->raw - (other << 16));
	};
	inline constexpr fix32 operator*(const i32 other) const {
		return fix32().setRaw(this->raw * other);
	};
	inline constexpr fix32 operator/(const i32 other) const {
		return fix32().setRaw(this->raw / other);
	};
	inline constexpr fix32 operator+=(const i32 other) {
		this->raw += (other << 16);
		return *this;
	};
	inline constexpr fix32 operator-=(const i32 other) {
		this->raw -= (other << 16);
		return *this;
	};
	inline constexpr fix32 operator*=(const i32 other) {
		this->raw *= other;
		return *this;
	};
	inline constexpr fix32 operator/=(const i32 other) {
		this->raw /= other;
		return *this;
	};
	inline constexpr bool operator>(const i32 other) const {
		return this->raw > (other << 16);
	};
	inline constexpr bool operator<(const i32 other) const {
		return this->raw < (other << 16);
	};
	inline constexpr bool operator>=(const i32 other) const {
		return this->raw >= (other << 16);
	};
	inline constexpr bool operator<=(const i32 other) const {
		return this->raw <= (other << 16);
	};

	// ======================== u32 arithmetics ========================
	inline constexpr fix32 operator+(const u32 other) const {
		return fix32().setRaw(this->raw + (other << 16));
	};
	inline constexpr fix32 operator-(const u32 other) const {
		return fix32().setRaw(this->raw - (other << 16));
	};
	inline constexpr fix32 operator*(const u32 other) const {
		return fix32().setRaw(this->raw * other);
	};
	inline constexpr fix32 operator/(const u32 other) const {
		return fix32().setRaw(this->raw / other);
	};
	inline constexpr fix32 operator+=(const u32 other) {
		this->raw += (other << 16);
		return *this;
	};
	inline constexpr fix32 operator-=(const u32 other) {
		this->raw -= (other << 16);
		return *this;
	};
	inline constexpr fix32 operator*=(const u32 other) {
		this->raw *= other;
		return *this;
	};
	inline constexpr fix32 operator/=(const u32 other) {
		this->raw /= other;
		return *this;
	};
	inline constexpr bool operator>(const u32 other) const {
		return this->raw > (other << 16);
	};
	inline constexpr bool operator<(const u32 other) const {
		return this->raw < (other << 16);
	};
	inline constexpr bool operator>=(const u32 other) const {
		return this->raw >= (other << 16);
	};
	inline constexpr bool operator<=(const u32 other) const {
		return this->raw <= (other << 16);
	};

	// ======================== f32 arithmetics ========================
	inline fix32 operator+(const f32 other) const {
		return fix32().setRaw(this->raw + (i32)(other * 65536));
	};
	inline constexpr fix32 operator-(const f32 other) const {
		return fix32().setRaw(this->raw - (i32)(other * 65536));
	};
	inline constexpr fix32 operator*(const f32 other) const {
		return fix32().setRaw((i32)(((i64)this->raw * (i64)(other * 65536)) >> 16));
	};
	inline constexpr fix32 operator/(const f32 other) const {
		return fix32().setRaw((i32)(((i64)this->raw << 16) / (i64)(other * 65536)));
	};
	inline constexpr fix32 operator+=(const f32 other) {
		this->raw += (i32)(other * 65536);
		return *this;
	};
	inline constexpr fix32 operator-=(const f32 other) {
		this->raw -= (i32)(other * 65536);
		return *this;
	};
	inline constexpr fix32 operator*=(const f32 other) {
		this->raw = (i32)(((i64)this->raw * (i64)(other * 65536)) >> 16);
		return *this;
	};
	inline constexpr fix32 operator/=(const f32 other) {
		this->raw = (i32)(((i64)this->raw << 16) / (i64)(other * 65536));
		return *this;
	};
	inline constexpr bool operator>(const f32 other) const {
		return *this > fix32(other);
	};
	inline constexpr bool operator<(const f32 other) const {
		return *this < fix32(other);
	};
	inline constexpr bool operator>=(const f32 other) const {
		return *this >= fix32(other);
	};
	inline constexpr bool operator<=(const f32 other) const {
		return *this <= fix32(other);
	};

	// ======================== f64 arithmetics ========================
	inline constexpr fix32 operator+(const f64 other) const {
		return fix32().setRaw(this->raw + (i32)(other * 65536));
	};
	inline constexpr fix32 operator-(const f64 other) const {
		return fix32().setRaw(this->raw - (i32)(other * 65536));
	};
	inline constexpr fix32 operator*(const f64 other) const {
		return fix32().setRaw((i32)(((i64)this->raw * (i64)(other * 65536)) >> 16));
	};
	inline constexpr fix32 operator/(const f64 other) const {
		return fix32().setRaw((i32)(((i64)this->raw << 16) / (i64)(other * 65536)));
	};
	inline constexpr fix32 operator+=(const f64 other) {
		this->raw += (i32)(other * 65536);
		return *this;
	};
	inline constexpr fix32 operator-=(const f64 other) {
		this->raw -= (i32)(other * 65536);
		return *this;
	};
	inline constexpr fix32 operator*=(const f64 other) {
		this->raw = (i32)(((i64)this->raw * (i64)(other * 65536)) >> 16);
		return *this;
	};
	inline constexpr fix32 operator/=(const f64 other) {
		this->raw = (i32)(((i64)this->raw << 16) / (i64)(other * 65536));
		return *this;
	};
	inline constexpr bool operator>(const f64 other) const {
		return *this > fix32(other);
	};
	inline constexpr bool operator<(const f64 other) const {
		return *this < fix32(other);
	};
	inline constexpr bool operator>=(const f64 other) const {
		return *this >= fix32(other);
	};
	inline constexpr bool operator<=(const f64 other) const {
		return *this <= fix32(other);
	};

	// ======================== fix64 arithmetics ========================
	inline constexpr fix32 operator+(const fix64 other) const {
		return fix32().setRaw(this->raw + (i32)(other.raw >> 16));
	};
	inline constexpr fix32 operator-(const fix64 other) const {
		return fix32().setRaw(this->raw - (i32)(other.raw >> 16));
	};
	inline constexpr fix32 operator*(const fix64 other) const {
		return fix32().setRaw((i32)(((i64)this->raw * (i64)other.raw) >> 32));
	};
	inline constexpr fix32 operator/(const fix64 other) const {
		return fix32().setRaw((i32)(((i64)this->raw << 32) / (i64)other.raw));
	};
	inline constexpr fix32 operator+=(const fix64 other) {
		this->raw += (i32)(other.raw >> 16);
		return *this;
	};
	inline constexpr fix32 operator-=(const fix64 other) {
		this->raw -= (i32)(other.raw >> 16);
		return *this;
	};
	inline constexpr fix32 operator*=(const fix64 other) {
		this->raw = (i32)(((i64)this->raw * (i64)other.raw) >> 32);
		return *this;
	};
	inline constexpr fix32 operator/=(const fix64 other) {
		this->raw = (i32)(((i64)this->raw << 32) / (i64)other.raw);
		return *this;
	};
	inline constexpr bool operator>(const fix64 other) const {
		return this->raw > (i32)(other.raw >> 16);
	};
	inline constexpr bool operator<(const fix64 other) const {
		return this->raw < (i32)(other.raw >> 16);
	};
	inline constexpr bool operator>=(const fix64 other) const {
		return this->raw >= (i32)(other.raw >> 16);
	};
	inline constexpr bool operator<=(const fix64 other) const {
		return this->raw <= (i32)(other.raw >> 16);
	};

	inline constexpr fix32 operator>>(const i32 other) const {
		return fix32().setRaw(this->raw >> other);
	};
	inline constexpr fix32 operator<<(const i32 other) const {
		return fix32().setRaw(this->raw << other);
	};
	inline constexpr fix32 operator-() const {
		return fix32().setRaw(-this->raw);
	};
	inline constexpr i32 sign() const {
		return (this->raw >> 31) * 2 + 1;
	};
	inline constexpr fix32 abs() const {
		return *this * sign();
	};
};

inline constexpr fix64::fix64(const fix32 v) {
	this->raw = (i64)v.raw << 16;
};
inline constexpr fix32 fix64::getfix32() const {
	return fix32().setRaw((i32)(this->raw >> 16));
}
inline constexpr fix64 fix64::operator+(const fix32 other) const {
	return fix64().setRaw(this->raw + (((i64)other.raw) << 16));
}
inline constexpr fix64 fix64::operator-(const fix32 other) const {
	return fix64().setRaw(this->raw - (((i64)other.raw) << 16));
}
inline constexpr fix64 fix64::operator*(const fix32 other) const {
	i32 sign = this->sign();
	i64 pos = this->raw * sign;
	i64 lo = (pos & 0xFFFFFFFFLL) * other.raw;
	i64 hi = (pos >> 32) * other.raw;
	hi <<= 16;
	lo >>= 16;
	return fix64().setRaw((hi + lo) * sign);
}
inline constexpr fix64 fix64::operator/(const fix32 other) const {
	i32 sign0 = this->sign();
	i32 sign1 = other.sign();
	i32 sign = sign0 * sign1;
	u64 raw0pos = this->raw * sign0;
	u64 raw1pos = other.raw * sign1;
	u64 hi = (raw0pos / raw1pos) << 16;
	u64 lo = (raw0pos % raw1pos) << 16;
	hi += lo / raw1pos;
	return fix64().setRaw((i64)hi * sign);
}
inline constexpr fix64 fix64::operator/(const f32 other) const {
	return *this / fix32(other);
};
