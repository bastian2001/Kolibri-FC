#include <Arduino.h>
#pragma once

class fixedPointInt64;

class fixedPointInt32 {
    // 16.16 fixed point
private:
    int32_t value = 0;

public:
    static fixedPointInt32 fromRaw(const uint32_t value);
    fixedPointInt32(const int32_t value = 0);
    fixedPointInt32(const float value);
    fixedPointInt32(const double value);
    fixedPointInt32(const int value);
    fixedPointInt32 setRaw(const int32_t value);
    int32_t         getRaw() const;
    float           getFloat() const;
    double          getDouble() const;
    int32_t         getInt() const;
    fixedPointInt64 toFixed64() const;
    fixedPointInt32 operator+(const fixedPointInt32 other) const;
    fixedPointInt32 operator-(const fixedPointInt32 other) const;
    fixedPointInt32 operator*(const fixedPointInt32 other) const;
    fixedPointInt32 operator/(const fixedPointInt32 other) const;
    fixedPointInt32 operator+=(const fixedPointInt32 other);
    fixedPointInt32 operator-=(const fixedPointInt32 other);
    fixedPointInt32 operator*=(const fixedPointInt32 other);
    fixedPointInt32 operator/=(const fixedPointInt32 other);
    fixedPointInt32 operator+(const int other) const;
    fixedPointInt32 operator-(const int other) const;
    fixedPointInt32 operator*(const int other) const;
    fixedPointInt32 operator*(const double other) const;
    fixedPointInt32 operator+=(const int other);
    fixedPointInt32 operator-=(const int other);
    fixedPointInt32 operator*=(const int other);
    fixedPointInt32 operator*=(const double other);
    fixedPointInt32 operator=(const int32_t other);
    fixedPointInt32 operator=(const int other);
    fixedPointInt32 operator=(const fixedPointInt64 other);
    fixedPointInt32 operator=(float other);
    fixedPointInt32 operator=(double other);
    bool            operator==(const int32_t other) const;
    bool            operator==(const fixedPointInt32 other) const;
    fixedPointInt32 operator*(const fixedPointInt64 other) const;
    fixedPointInt64 multiply64(const fixedPointInt64 other) const;
    bool            operator<(const fixedPointInt32 other) const;
    bool            operator>(const fixedPointInt32 other) const;
    bool            operator<=(const fixedPointInt32 other) const;
    bool            operator>=(const fixedPointInt32 other) const;
    fixedPointInt32 operator%=(const fixedPointInt32 other);
    fixedPointInt32 operator>>(const int32_t other) const;
    fixedPointInt32 operator<<(const int32_t other) const;
    fixedPointInt32 operator-() const;
};

class fixedPointInt64 {
    // 48.16 fixed point
private:
    int64_t value = 0;

public:
    static fixedPointInt64 fromRaw(const uint32_t value);
    fixedPointInt64(const int32_t value = 0);
    fixedPointInt64(const float value);
    fixedPointInt64 setRaw(const int64_t value);
    int64_t         getRaw() const;
    float           getFloat() const;
    double          getDouble() const;
    int32_t         getInt() const;
    fixedPointInt32 toFixed32() const;
    fixedPointInt64 operator+(const fixedPointInt64 other) const;
    fixedPointInt64 operator+(const fixedPointInt32 other) const;
    fixedPointInt64 operator+=(const fixedPointInt32 other);
    fixedPointInt64 operator*(const fixedPointInt64 other) const;
    fixedPointInt32 operator*(const fixedPointInt32 other) const;
    fixedPointInt64 multiply64(const fixedPointInt32 other) const;
    fixedPointInt64 operator=(const int32_t other);
    fixedPointInt64 operator=(const int other);
    fixedPointInt64 operator=(const int64_t other);
    fixedPointInt64 operator=(const fixedPointInt32 other);
    fixedPointInt64 operator=(const float other);
    fixedPointInt64 operator=(const double other);
};