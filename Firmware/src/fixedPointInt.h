#include <Arduino.h>
#pragma once

class fix64;

class fix32 {
	// 16.16 fixed point
private:
	i32 value = 0;

public:
	static fix32 fromRaw(const i32 value);
	fix32(const i32 value = 0);
	fix32(const f32 value);
	fix32(const f64 value);
	fix32(const int value);
	fix32 setRaw(const i32 value);
	i32			getRaw() const;
	f32			getf32() const;
	f64			getf64() const;
	i32			getInt() const;
	fix64 toFixed64() const;
	fix32 operator+(const fix32 other) const;
	fix32 operator-(const fix32 other) const;
	fix32 operator*(const fix32 other) const;
	fix32 operator/(const fix32 other) const;
	fix32 operator+=(const fix32 other);
	fix32 operator-=(const fix32 other);
	fix32 operator*=(const fix32 other);
	fix32 operator/=(const fix32 other);
	fix32 operator+(const int other) const;
	fix32 operator-(const int other) const;
	fix32 operator*(const int other) const;
	fix32 operator*(const f64 other) const;
	fix32 operator+=(const int other);
	fix32 operator-=(const int other);
	fix32 operator*=(const int other);
	fix32 operator*=(const f64 other);
	fix32 operator=(const i32 other);
	fix32 operator=(const int other);
	fix32 operator=(const fix64 other);
	fix32 operator=(f32 other);
	fix32 operator=(f64 other);
	bool			operator==(const i32 other) const;
	bool			operator==(const fix32 other) const;
	fix32 operator*(const fix64 other) const;
	fix64 multiply64(const fix64 other) const;
	bool			operator<(const fix32 other) const;
	bool			operator>(const fix32 other) const;
	bool			operator<=(const fix32 other) const;
	bool			operator>=(const fix32 other) const;
	fix32 operator%=(const fix32 other);
	fix32 operator>>(const i32 other) const;
	fix32 operator<<(const i32 other) const;
	fix32 operator-() const;
};

class fix64 {
	// 48.16 fixed point
private:
	i64 value = 0;

public:
	static fix64 fromRaw(const i64 value);
	fix64(const i32 value = 0);
	fix64(const int value);
	fix64(const f32 value);
	fix64 setRaw(const i64 value);
	i64			getRaw() const;
	f32			getf32() const;
	f64			getf64() const;
	i32			getInt() const;
	fix32 toFixed32() const;
	fix64 operator+(const fix64 other) const;
	fix64 operator-(const fix64 other) const;
	fix64 operator+(const fix32 other) const;
	fix64 operator-(const fix32 other) const;
	fix64 operator+=(const fix32 other);
	fix64 operator*(const fix64 other) const;
	fix32 operator*(const fix32 other) const;
	fix64 operator*(const int other) const;
	fix64 multiply64(const fix32 other) const;
	fix64 operator=(const i32 other);
	fix64 operator=(const int other);
	fix64 operator=(const i64 other);
	fix64 operator=(const fix32 other);
	fix64 operator=(const f32 other);
	fix64 operator=(const f64 other);
	fix64 operator>>(const i32 other) const;
	fix64 operator-() const;
};