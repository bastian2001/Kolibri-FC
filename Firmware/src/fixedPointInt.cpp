#include "global.h"

// fix32 fix32::fromRaw(const i32 v) {
// 	fix32 result;
// 	return result.setRaw(v);
// }
// fix32::fix32(const i32 v) {
// 	this->value = v << 16;
// }
// fix32::fix32(const f32 v) {
// 	this->value = (i32)(v * 65536);
// }
// fix32::fix32(const f64 v) {
// 	this->value = (i32)(v * 65536);
// }
// fix32::fix32(const int v) {
// 	this->value = v << 16;
// }
// fix32 fix32::setRaw(const i32 v) {
// 	this->value = v;
// 	return *this;
// }
// i32 fix32::getRaw() const {
// 	return this->value;
// }
// f32 fix32::getf32() const {
// 	return (f32)this->value / 65536;
// }
// f64 fix32::getf64() const {
// 	return (f64)this->value / 65536;
// }
// i32 fix32::getInt() const {
// 	return (i32)(this->value >> 16);
// }
// fix64 fix32::toFixed64() const {
// 	fix64 result;
// 	return result.setRaw(((i64)this->value) << 16);
// }
// fix32 fix32::operator+(const fix32 other) const {
// 	fix32 result;
// 	return result.setRaw(this->value + other.getRaw());
// }
// fix32 fix32::operator-(const fix32 other) const {
// 	fix32 result;
// 	return result.setRaw(this->value - other.getRaw());
// }
// fix32 fix32::operator*(const fix32 other) const {
// 	fix32 result;
// 	return result.setRaw((i32)(((i64)this->value * (i64)other.getRaw()) >> 16));
// }
// fix32 fix32::operator/(const fix32 other) const {
// 	fix32 result;
// 	return result.setRaw((i32)((((i64)this->value) << 16) / (i64)other.getRaw()));
// }
// fix32 fix32::operator+=(const fix32 other) {
// 	this->value += other.getRaw();
// 	return *this;
// }
// fix32 fix32::operator-=(const fix32 other) {
// 	this->value -= other.getRaw();
// 	return *this;
// }
// fix32 fix32::operator*=(const fix32 other) {
// 	this->value = (i32)(((i64)this->value * (i64)other.getRaw()) >> 16);
// 	return *this;
// }
// fix32 fix32::operator/=(const fix32 other) {
// 	this->value = (i32)(((i64)this->value << 16) / (i64)other.getRaw());
// 	return *this;
// }
// fix32 fix32::operator+(const int other) const {
// 	fix32 result;
// 	return result.setRaw(this->value + (other << 16));
// }
// fix32 fix32::operator-(const int other) const {
// 	fix32 result;
// 	return result.setRaw(this->value - (other << 16));
// }
// fix32 fix32::operator*(const int other) const {
// 	fix32 result;
// 	return result.setRaw(this->value * other);
// }
// fix32 fix32::operator*(const f64 other) const {
// 	return *this * fix32(other);
// }
// fix32 fix32::operator+=(const int other) {
// 	this->value += (other << 16);
// 	return *this;
// }
// fix32 fix32::operator-=(const int other) {
// 	this->value -= (other << 16);
// 	return *this;
// }
// fix32 fix32::operator*=(const int other) {
// 	this->value *= other;
// 	return *this;
// }
// fix32 fix32::operator*=(const f64 other) {
// 	return *this *= fix32(other);
// }
// fix32 fix32::operator=(const i32 other) {
// 	this->value = (other << 16);
// 	return *this;
// }
// fix32 fix32::operator=(const int other) {
// 	this->value = (other << 16);
// 	return *this;
// }
// fix32 fix32::operator=(const fix64 other) {
// 	this->value = (i32)(other.getRaw() >> 16);
// 	return *this;
// }
// fix32 fix32::operator=(const f32 other) {
// 	this->value = (i32)(other * 65536);
// 	return *this;
// }
// fix32 fix32::operator=(const f64 other) {
// 	this->value = (i32)(other * 65536);
// 	return *this;
// }
// bool fix32::operator==(const i32 other) const {
// 	return this->value == (other << 16);
// }
// bool fix32::operator==(const fix32 other) const {
// 	return this->value == other.getRaw();
// }
// fix32 fix32::operator*(const fix64 other) const {
// 	fix32 result;
// 	return result.setRaw((i32)(((i64)this->value * (i64)other.getRaw()) >> 32));
// }
// fix64 fix32::multiply64(const fix64 other) const {
// 	fix64 result;
// 	return result.setRaw((i32)(((i64)this->value * (i64)other.getRaw()) >> 32));
// }
// bool fix32::operator<(const fix32 other) const {
// 	return this->value < other.getRaw();
// }
// bool fix32::operator>(const fix32 other) const {
// 	return this->value > other.getRaw();
// }
// bool fix32::operator<=(const fix32 other) const {
// 	return this->value <= other.getRaw();
// }
// bool fix32::operator>=(const fix32 other) const {
// 	return this->value >= other.getRaw();
// }
// fix32 fix32::operator%=(const fix32 other) {
// 	this->value %= other.getRaw();
// 	return *this;
// }
// fix32 fix32::operator>>(const i32 other) const {
// 	return fix32::fromRaw(this->value >> other);
// }
// fix32 fix32::operator<<(const i32 other) const {
// 	return fix32::fromRaw(this->value << other);
// }
// fix32 fix32::operator-() const {
// 	return fix32::fromRaw(-this->value);
// }

// //================================================ fix64 ================================================

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
	i64 raw1	= this->value;
	i64 raw2	= other.getRaw();
	i64 pos1	= raw1 >= 0 ? raw1 : -raw1;
	i64 pos2	= raw2 >= 0 ? raw2 : -raw2;
	u64 big		= (pos1 >> 32) * (pos2 >> 32);
	u64 small	= (pos1 & 0xFFFFFFFF) * (pos2 & 0xFFFFFFFF);
	u64 med		= (pos1 >> 32) * (pos2 & 0xFFFFFFFF) + (pos1 & 0xFFFFFFFF) * (pos2 >> 32);
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