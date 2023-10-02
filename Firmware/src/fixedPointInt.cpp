#include "global.h"

fixedPointInt32 fixedPointInt32::fromRaw(const uint32_t v) {
    fixedPointInt32 result;
    return result.setRaw(v);
}
fixedPointInt32 fixedPointInt32::from(const float v) {
    fixedPointInt32 result;
    return result.setRaw((int32_t)(v * 65536));
}
fixedPointInt32 fixedPointInt32::from(const double v) {
    fixedPointInt32 result;
    return result.setRaw((int32_t)(v * 65536));
}
fixedPointInt32 fixedPointInt32::from(const int32_t v) {
    fixedPointInt32 result;
    return result.setRaw(v << 16);
}
fixedPointInt32 fixedPointInt32::from(const int v) {
    fixedPointInt32 result;
    return result.setRaw(v << 16);
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
    return result.setRaw((int64_t)this->value);
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
    return result.setRaw((int32_t)(((int64_t)this->value << 16) / (int64_t)other.getRaw()));
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
    return *this * fixedPointInt32::from(other);
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
    return *this *= fixedPointInt32::from(other);
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
    this->value = (int32_t)(other.getRaw());
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
    return result.setRaw((int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16));
}
fixedPointInt64 fixedPointInt32::multiply64(const fixedPointInt64 other) const {
    fixedPointInt64 result;
    return result.setRaw((int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16));
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

fixedPointInt64 fixedPointInt64::fromRaw(uint32_t v) {
    fixedPointInt64 result;
    return result.setRaw(v);
}
fixedPointInt64::fixedPointInt64(const int32_t v) {
    this->value = v << 16;
}
fixedPointInt64 fixedPointInt64::setRaw(const int64_t v) {
    this->value = v;
    return *this;
}
int64_t fixedPointInt64::getRaw() const {
    return this->value;
}
float fixedPointInt64::getFloat() const {
    return (float)this->value / 65536;
}
double fixedPointInt64::getDouble() const {
    return (double)this->value / 65536;
}
int32_t fixedPointInt64::getInt() const {
    return (int32_t)(this->value >> 16);
}
fixedPointInt32 fixedPointInt64::toFixed32() const {
    fixedPointInt32 result;
    return result.setRaw((int32_t)this->value);
}
fixedPointInt64 fixedPointInt64::operator+(const fixedPointInt64 other) const {
    fixedPointInt64 result;
    return result.setRaw(this->value + other.getRaw());
}
fixedPointInt64 fixedPointInt64::operator+(const fixedPointInt32 other) const {
    fixedPointInt64 result;
    return result.setRaw(this->value + other.getRaw());
}
fixedPointInt64 fixedPointInt64::operator+=(const fixedPointInt32 other) {
    this->value += other.getRaw();
    return *this;
}
fixedPointInt64 fixedPointInt64::operator*(const fixedPointInt64 other) const {
    fixedPointInt64 result;
    return result.setRaw((int64_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16));
}
fixedPointInt32 fixedPointInt64::operator*(const fixedPointInt32 other) const {
    fixedPointInt32 result;
    return result.setRaw((int32_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16));
}
fixedPointInt64 fixedPointInt64::multiply64(const fixedPointInt32 other) const {
    fixedPointInt64 result;
    return result.setRaw((int64_t)(((int64_t)this->value * (int64_t)other.getRaw()) >> 16));
}
fixedPointInt64 fixedPointInt64::operator=(const int32_t other) {
    this->value = other << 16;
    return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const int other) {
    this->value = other << 16;
    return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const int64_t other) {
    this->value = other;
    return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const fixedPointInt32 other) {
    this->value = (int64_t)(other.getRaw());
    return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const float other) {
    this->value = (int64_t)(other * 65536);
    return *this;
}
fixedPointInt64 fixedPointInt64::operator=(const double other) {
    this->value = (int64_t)(other * 65536);
    return *this;
}
