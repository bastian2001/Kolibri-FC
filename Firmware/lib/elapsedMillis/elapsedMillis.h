/* Elapsed time types - for easy-to-use measurements of elapsed time
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2011 PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * This file is basically identical to github.com/pfeerick/elapsedMillis, except
 * that it is using a micros32 function instead of a micros function. Since micros()
 * is 32 bit, there is no reason to first get a 64 bit value and then cast it to 32 bit.
 * This modification is only compatible with the Pico SDK.
 */

#ifndef elapsedMillis_h
#define elapsedMillis_h
#ifdef __cplusplus

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

/**
 * @brief returns the number of microseconds since boot. Faster than micros().
 *
 * @return uint32_t microseconds since boot
 */
static inline uint32_t micros32() {
	return PICO_DEFAULT_TIMER_INSTANCE()->timerawl;
}

class elapsedMicros {
private:
	unsigned long us;

public:
	elapsedMicros(void) { us = micros32(); }
	elapsedMicros(unsigned long val) { us = micros32() - val; }
	elapsedMicros(const elapsedMicros &orig) { us = orig.us; }
	operator unsigned long() const { return micros32() - us; }
	elapsedMicros &operator=(const elapsedMicros &rhs) {
		us = rhs.us;
		return *this;
	}
	elapsedMicros &operator=(unsigned long val) {
		us = micros32() - val;
		return *this;
	}
	elapsedMicros &operator-=(unsigned long val) {
		us += val;
		return *this;
	}
	elapsedMicros &operator+=(unsigned long val) {
		us -= val;
		return *this;
	}
	elapsedMicros operator-(int val) const {
		elapsedMicros r(*this);
		r.us += val;
		return r;
	}
	elapsedMicros operator-(unsigned int val) const {
		elapsedMicros r(*this);
		r.us += val;
		return r;
	}
	elapsedMicros operator-(long val) const {
		elapsedMicros r(*this);
		r.us += val;
		return r;
	}
	elapsedMicros operator-(unsigned long val) const {
		elapsedMicros r(*this);
		r.us += val;
		return r;
	}
	elapsedMicros operator+(int val) const {
		elapsedMicros r(*this);
		r.us -= val;
		return r;
	}
	elapsedMicros operator+(unsigned int val) const {
		elapsedMicros r(*this);
		r.us -= val;
		return r;
	}
	elapsedMicros operator+(long val) const {
		elapsedMicros r(*this);
		r.us -= val;
		return r;
	}
	elapsedMicros operator+(unsigned long val) const {
		elapsedMicros r(*this);
		r.us -= val;
		return r;
	}
};

class elapsedMillis {
private:
	unsigned long ms;

public:
	elapsedMillis(void) { ms = millis(); }
	elapsedMillis(unsigned long val) { ms = millis() - val; }
	elapsedMillis(const elapsedMillis &orig) { ms = orig.ms; }
	operator unsigned long() const { return millis() - ms; }
	elapsedMillis &operator=(const elapsedMillis &rhs) {
		ms = rhs.ms;
		return *this;
	}
	elapsedMillis &operator=(unsigned long val) {
		ms = millis() - val;
		return *this;
	}
	elapsedMillis &operator-=(unsigned long val) {
		ms += val;
		return *this;
	}
	elapsedMillis &operator+=(unsigned long val) {
		ms -= val;
		return *this;
	}
	elapsedMillis operator-(int val) const {
		elapsedMillis r(*this);
		r.ms += val;
		return r;
	}
	elapsedMillis operator-(unsigned int val) const {
		elapsedMillis r(*this);
		r.ms += val;
		return r;
	}
	elapsedMillis operator-(long val) const {
		elapsedMillis r(*this);
		r.ms += val;
		return r;
	}
	elapsedMillis operator-(unsigned long val) const {
		elapsedMillis r(*this);
		r.ms += val;
		return r;
	}
	elapsedMillis operator+(int val) const {
		elapsedMillis r(*this);
		r.ms -= val;
		return r;
	}
	elapsedMillis operator+(unsigned int val) const {
		elapsedMillis r(*this);
		r.ms -= val;
		return r;
	}
	elapsedMillis operator+(long val) const {
		elapsedMillis r(*this);
		r.ms -= val;
		return r;
	}
	elapsedMillis operator+(unsigned long val) const {
		elapsedMillis r(*this);
		r.ms -= val;
		return r;
	}
};

class elapsedSeconds {
private:
	unsigned long ms;
	const unsigned long msPerSec = 1000;

public:
	elapsedSeconds(void) { ms = millis(); }
	elapsedSeconds(unsigned long val) { ms = millis() - val * msPerSec; }
	elapsedSeconds(const elapsedSeconds &orig) { ms = orig.ms; }
	operator unsigned long() const { return (millis() - ms) / msPerSec; }
	elapsedSeconds &operator=(const elapsedSeconds &rhs) {
		ms = rhs.ms;
		return *this;
	}
	elapsedSeconds &operator=(unsigned long val) {
		ms = millis() - val * msPerSec;
		return *this;
	}
	elapsedSeconds &operator-=(unsigned long val) {
		ms += val * msPerSec;
		return *this;
	}
	elapsedSeconds &operator+=(unsigned long val) {
		ms -= val * msPerSec;
		return *this;
	}
	elapsedSeconds operator-(int val) const {
		elapsedSeconds r(*this);
		r.ms += val * msPerSec;
		return r;
	}
	elapsedSeconds operator-(unsigned int val) const {
		elapsedSeconds r(*this);
		r.ms += val * msPerSec;
		return r;
	}
	elapsedSeconds operator-(long val) const {
		elapsedSeconds r(*this);
		r.ms += val * msPerSec;
		return r;
	}
	elapsedSeconds operator-(unsigned long val) const {
		elapsedSeconds r(*this);
		r.ms += val * msPerSec;
		return r;
	}
	elapsedSeconds operator+(int val) const {
		elapsedSeconds r(*this);
		r.ms -= val * msPerSec;
		return r;
	}
	elapsedSeconds operator+(unsigned int val) const {
		elapsedSeconds r(*this);
		r.ms -= val * msPerSec;
		return r;
	}
	elapsedSeconds operator+(long val) const {
		elapsedSeconds r(*this);
		r.ms -= val * msPerSec;
		return r;
	}
	elapsedSeconds operator+(unsigned long val) const {
		elapsedSeconds r(*this);
		r.ms -= val * msPerSec;
		return r;
	}
};

#endif // __cplusplus
#endif // elapsedMillis_h
