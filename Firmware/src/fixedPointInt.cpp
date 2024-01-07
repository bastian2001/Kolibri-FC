#include "global.h"

fixedPointInt32 fixedPointInt32::fromRaw(const int32_t v) {
	fixedPointInt32 result;
	return result.setRaw(v);
}
fixedPointInt32::fixedPointInt32(const int32_t v) {
	this->value = v << 16;
}
fixedPointInt32::fixedPointInt32(const float v) {
	this->value = (int32_t)(v * 65536);
}
fixedPointInt32::fixedPointInt32(const double v) {
	this->value = (int32_t)(v * 65536);
}
fixedPointInt32::fixedPointInt32(const int v) {
	this->value = v << 16;
}
fixedPointInt32 fixedPointInt32::setRaw(const int32_t v) {
	this->value = v;
	return *this;
}
int32_t fixedPointInt32::getRaw() const {
	return this->value;
}
float fixedPointInt32::getFloat() const {
	return (float)this->value / 65536;
}
double fixedPointInt32::getDouble() const {
	return (double)this->value / 65536;
}
int32_t fixedPointInt32::getInt() const {
	return (int32_t)(this->value >> 16);
}
fixedPointInt64 fixedPointInt32::toFixed64() const {
	fixedPointInt64 result;
	return result.setRaw(((int64_t)this->value) << 16);
}
fixedPointInt32 fixedPointInt32::operator+(const fixedPointInt32 other) const {
	fixedPointInt32 result;
	return result.setRaw(this->value + other.getRaw());
}
fixedPointInt32 fixedPointInt32::operator-(const fixedPointInt32 other) const {
	fixedPointInt32 result;
	return result.setRaw(this->value - other.getRaw());
}
fixedPointInt32 fixedPointInt32::operator*(const fixedPointInt32 other) const {
	fixedPointInt32 result;
	return result.setRaw((int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16));
}
fixedPointInt32 fixedPointInt32::operator/(const fixedPointInt32 other) const {
	fixedPointInt32 result;
	return result.setRaw((int32_t)((((int64_t)this->value) << 16) / (int64_t)other.getRaw()));
}
fixedPointInt32 fixedPointInt32::operator+=(const fixedPointInt32 other) {
	this->value += other.getRaw();
	return *this;
}
fixedPointInt32 fixedPointInt32::operator-=(const fixedPointInt32 other) {
	this->value -= other.getRaw();
	return *this;
}
fixedPointInt32 fixedPointInt32::operator*=(const fixedPointInt32 other) {
	this->value = (int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16);
	return *this;
}
fixedPointInt32 fixedPointInt32::operator/=(const fixedPointInt32 other) {
	this->value = (int32_t)(((int64_t)this->value << 16) / (int64_t)other.getRaw());
	return *this;
}
fixedPointInt32 fixedPointInt32::operator+(const int other) const {
	fixedPointInt32 result;
	return result.setRaw(this->value + (other << 16));
}
fixedPointInt32 fixedPointInt32::operator-(const int other) const {
	fixedPointInt32 result;
	return result.setRaw(this->value - (other << 16));
}
fixedPointInt32 fixedPointInt32::operator*(const int other) const {
	fixedPointInt32 result;
	return result.setRaw(this->value * other);
}
fixedPointInt32 fixedPointInt32::operator*(const double other) const {
	return *this * fixedPointInt32(other);
}
fixedPointInt32 fixedPointInt32::operator+=(const int other) {
	this->value += (other << 16);
	return *this;
}
fixedPointInt32 fixedPointInt32::operator-=(const int other) {
	this->value -= (other << 16);
	return *this;
}
fixedPointInt32 fixedPointInt32::operator*=(const int other) {
	this->value *= other;
	return *this;
}
fixedPointInt32 fixedPointInt32::operator*=(const double other) {
	return *this *= fixedPointInt32(other);
}
fixedPointInt32 fixedPointInt32::operator=(const int32_t other) {
	this->value = (other << 16);
	return *this;
}
fixedPointInt32 fixedPointInt32::operator=(const int other) {
	this->value = (other << 16);
	return *this;
}
fixedPointInt32 fixedPointInt32::operator=(const fixedPointInt64 other) {
	this->value = (int32_t)(other.getRaw() >> 16);
	return *this;
}
fixedPointInt32 fixedPointInt32::operator=(const float other) {
	this->value = (int32_t)(other * 65536);
	return *this;
}
fixedPointInt32 fixedPointInt32::operator=(const double other) {
	this->value = (int32_t)(other * 65536);
	return *this;
}
bool fixedPointInt32::operator==(const int32_t other) const {
	return this->value == (other << 16);
}
bool fixedPointInt32::operator==(const fixedPointInt32 other) const {
	return this->value == other.getRaw();
}
fixedPointInt32 fixedPointInt32::operator*(const fixedPointInt64 other) const {
	fixedPointInt32 result;
	return result.setRaw((int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 32));
}
fixedPointInt64 fixedPointInt32::multiply64(const fixedPointInt64 other) const {
	fixedPointInt64 result;
	return result.setRaw((int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 32));
}
bool fixedPointInt32::operator<(const fixedPointInt32 other) const {
	return this->value < other.getRaw();
}
bool fixedPointInt32::operator>(const fixedPointInt32 other) const {
	return this->value > other.getRaw();
}
bool fixedPointInt32::operator<=(const fixedPointInt32 other) const {
	return this->value <= other.getRaw();
}
bool fixedPointInt32::operator>=(const fixedPointInt32 other) const {
	return this->value >= other.getRaw();
}
fixedPointInt32 fixedPointInt32::operator%=(const fixedPointInt32 other) {
	this->value %= other.getRaw();
	return *this;
}
fixedPointInt32 fixedPointInt32::operator>>(const int32_t other) const {
	return fixedPointInt32::fromRaw(this->value >> other);
}
fixedPointInt32 fixedPointInt32::operator<<(const int32_t other) const {
	return fixedPointInt32::fromRaw(this->value << other);
}
fixedPointInt32 fixedPointInt32::operator-() const {
	return fixedPointInt32::fromRaw(-this->value);
}

//================================================= fixedPointInt64 =================================================

fixedPointInt64 fixedPointInt64::fromRaw(int64_t v) {
	fixedPointInt64 result;
	return result.setRaw(v);
}
fixedPointInt64::fixedPointInt64(const int32_t v) {
	this->value = (int64_t)v << 32;
}
fixedPointInt64::fixedPointInt64(const int v) {
	this->value = (int64_t)v << 32;
}
fixedPointInt64::fixedPointInt64(const float v) {
	this->value = (int64_t)(v * 4294967296);
}
fixedPointInt64 fixedPointInt64::setRaw(const int64_t v) {
	this->value = v;
	return *this;
}
int64_t fixedPointInt64::getRaw() const {
	return this->value;
}
float fixedPointInt64::getFloat() const {
	return (float)this->value / 4294967296;
}
double fixedPointInt64::getDouble() const {
	return (double)this->value / 4294967296;
}
int32_t fixedPointInt64::getInt() const {
	return (int32_t)(this->value >> 32);
}
fixedPointInt32 fixedPointInt64::toFixed32() const {
	fixedPointInt32 result;
	return result.setRaw((int32_t)(this->value >> 16));
}
fixedPointInt64 fixedPointInt64::operator+(const fixedPointInt64 other) const {
	fixedPointInt64 result;
	return result.setRaw(this->value + other.getRaw());
}
fixedPointInt64 fixedPointInt64::operator-(const fixedPointInt64 other) const {
	return fixedPointInt64::fromRaw(this->value - other.getRaw());
}
fixedPointInt64 fixedPointInt64::operator+(const fixedPointInt32 other) const {
	fixedPointInt64 result;
	return result.setRaw(this->value + (((int64_t)other.getRaw()) << 16));
}
fixedPointInt64 fixedPointInt64::operator-(const fixedPointInt32 other) const {
	fixedPointInt64 result;
	return result.setRaw(this->value - (((int64_t)other.getRaw()) << 16));
}
fixedPointInt64 fixedPointInt64::operator+=(const fixedPointInt32 other) {
	this->value += ((int64_t)other.getRaw()) << 16;
	return *this;
}
fixedPointInt64 fixedPointInt64::operator*(const fixedPointInt64 other) const {
	int64_t	 raw1	 = this->value;
	int64_t	 raw2	 = other.getRaw();
	int64_t	 pos1	 = raw1 >= 0 ? raw1 : -raw1;
	int64_t	 pos2	 = raw2 >= 0 ? raw2 : -raw2;
	uint64_t big	 = (pos1 >> 32) * (pos2 >> 32);
	uint64_t small	 = (pos1 & 0xFFFFFFFF) * (pos2 & 0xFFFFFFFF);
	uint64_t med	 = (pos1 >> 32) * (pos2 & 0xFFFFFFFF) + (pos1 & 0xFFFFFFFF) * (pos2 >> 32);
	int64_t	 result2 = med + ((small >> 32) & 0xFFFFFFFF) + (big << 32);
	if (raw1 < 0) result2 = -result2;
	if (raw2 < 0) result2 = -result2;
	return fixedPointInt64::fromRaw(result2);
}
fixedPointInt32 fixedPointInt64::operator*(const fixedPointInt32 other) const {
	fixedPointInt32 result;
	return result.setRaw((int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 32));
}
fixedPointInt64 fixedPointInt64::operator*(const int other) const {
	fixedPointInt64 result;
	return result.setRaw(this->value * other);
}
fixedPointInt64 fixedPointInt64::multiply64(const fixedPointInt32 other) const {
	fixedPointInt64 result;
	return result.setRaw((int64_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16));
}
fixedPointInt64 fixedPointInt64::operator=(const int32_t other) {
	this->value = (int64_t)other << 32;
	return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const int other) {
	this->value = (int64_t)other << 32;
	return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const int64_t other) {
	this->value = other << 32;
	return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const fixedPointInt32 other) {
	this->value = ((int64_t)(other.getRaw())) << 16;
	return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const float other) {
	this->value = (int64_t)(other * 4294967296);
	return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const double other) {
	this->value = (int64_t)(other * 4294967296);
	return *this;
}
fixedPointInt64 fixedPointInt64::operator>>(const int32_t other) const {
	return fixedPointInt64::fromRaw(this->value >> other);
}
fixedPointInt64 fixedPointInt64::operator-() const {
	return fixedPointInt64::fromRaw(-this->value);
}