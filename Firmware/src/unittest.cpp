#include "unittest.h"
#include "fixedPointInt.h"
#include "ringbuffer.h"

u32 ExpectBase::failed = false;
u32 ExpectBase::succeeded = false;
bool ExpectBase::silentLogging = false;
template <>
const char *Fmt<i32>::f = "%6d";
template <>
const char *Fmt<u32>::f = "%6u";
template <>
const char *Fmt<f32>::f = "%6.2f";
template <>
const char *Fmt<f64>::f = "%6.2lf";
template <>
const char *Fmt<i16>::f = "%6d";
template <>
const char *Fmt<u16>::f = "%6u";
template <>
const char *Fmt<i8>::f = "%6d";
template <>
const char *Fmt<u8>::f = "%6u";
template <>
const char *Fmt<bool>::f = "%6d";
template <>
const char *Fmt<size_t>::f = "%6u";
template <>
const char *Fmt<i64>::f = "%6lld";
template <>
const char *Fmt<u64>::f = "%6llu";

bool testRingBuffer() {
	// test ringbuffer
	RingBuffer<u32> rb(10);
	Expect(rb.isEmpty()).withIndex(0).toEqual(true);
	Expect(rb.isFull()).withIndex(1).toEqual(false);
	Expect(rb.freeSpace()).withIndex(2).toEqual(10);
	for (int i = 0; i < 9; i++) {
		rb.push(i);
	}
	Expect(rb.isEmpty()).withIndex(3).toEqual(false);
	Expect(rb.isFull()).withIndex(4).toEqual(false);
	Expect(rb.freeSpace()).withIndex(5).toEqual(1);
	Expect(rb.pop()).withIndex(6).toEqual(0);
	Expect(rb.pop()).withIndex(7).toEqual(1);
	Expect(rb[2]).withIndex(8).toEqual(4);
	Expect(rb.itemCount()).withIndex(9).toEqual(7);
	rb.push(9);
	rb.push(10);
	Expect(rb.isFull()).withIndex(10).toEqual(false);
	Expect(rb.freeSpace()).withIndex(11).toEqual(1);
	Expect(rb.itemCount()).withIndex(12).toEqual(9);
	Expect(rb.isEmpty()).withIndex(13).toEqual(false);
	rb.push(11);
	Expect(rb.isFull()).withIndex(14).toEqual(true);
	Expect(rb.freeSpace()).withIndex(15).toEqual(0);
	Expect(rb.itemCount()).withIndex(16).toEqual(10);
	for (int i = 0; i < 10; i++) {
		rb.push(i);
	}
	Expect(rb.isFull()).withIndex(17).toEqual(true);
	Expect(rb.freeSpace()).withIndex(18).toEqual(0);
	Expect(rb.isEmpty()).withIndex(19).toEqual(false);
	Expect(rb.itemCount()).withIndex(20).toEqual(10);
	Expect(rb.pop()).withIndex(21).toEqual(0);
	rb.push(10);
	rb.erase(3);
	Expect(rb.itemCount()).withIndex(22).toEqual(7);
	u32 output[10];
	rb.copyToArray(output, 0, 10);
	Expect(output[0]).withIndex(23).toEqual(4);
	Expect(output[6]).withIndex(24).toEqual(10);
	rb.resize(5);
	Expect(rb.itemCount()).withIndex(25).toEqual(5);
	Expect(rb.isFull()).withIndex(26).toEqual(true);
	Expect(rb.isEmpty()).withIndex(27).toEqual(false);
	Expect(rb.freeSpace()).withIndex(28).toEqual(0);
	Expect(rb.pop()).withIndex(29).toEqual(4);
	return ExpectBase::printResults(true, "Ringbuffer");
}

bool testFixedPoint() {
	fix32 a = fix32::fromRaw(0b101 << 15);
	Expect(a.getf32()).withIndex(0).toEqual(2.5);
	a = fix32(3);
	Expect(a.getRaw()).withIndex(1).toEqual(3 << 16);
	a = fix32(2.5f);
	Expect(a.getRaw()).withIndex(2).toEqual(0b101 << 15);
	a = fix32(2.5);
	Expect(a.getRaw()).withIndex(3).toEqual(0b101 << 15);
	a.setRaw(0b111 << 15);
	Expect(a.getf32()).withIndex(4).toEqual(3.5);
	Expect(a.getf64()).withIndex(5).toEqual(3.5);
	Expect(a.getInt()).withIndex(6).toEqual(3);
	Expect(a.toFixed64().getRaw()).withIndex(7).toEqual(0b111LL << 31);
	fix32 b = 2.75;
	Expect((a + b).getf32()).withIndex(8).toEqual(6.25);
	Expect((a - b).getf32()).withIndex(9).toEqual(0.75);
	Expect((a * b).getf32()).withIndex(10).toEqual(9.625);
	Expect((a / b).getf32()).withIndex(11).toBeGreaterThanOrEqual(1.27);
	Expect((a / b).getf32()).withIndex(12).toBeLessThanOrEqual(1.28);
	Expect((a / -b).getf32()).withIndex(13).toBeLessThanOrEqual(-1.27);
	Expect((a / -b).getf32()).withIndex(14).toBeGreaterThanOrEqual(-1.28);
	b = b / 2;
	Expect(b.getf32()).withIndex(15).toEqual(1.375);
	a += b;
	Expect(a.getf32()).withIndex(16).toEqual(4.875);
	b -= a;
	Expect(b.getf32()).withIndex(17).toEqual(-3.5);
	b *= a;
	Expect(b.getf32()).withIndex(18).toEqual(-17.0625);
	fix32 c = 0.00001526; // 1/65536, smallest possible value
	Expect(c.getRaw()).withIndex(19).toEqual(1);
	c += 0.00001525; // slightly smaller than smallest value, expect to add 0
	Expect(c.getRaw()).withIndex(20).toEqual(1);
	b = -2.5;
	a = 3.5;
	b /= a;
	Expect(b.getf32()).withIndex(21).toBeGreaterThanOrEqual(-0.7143);
	Expect(b.getf32()).withIndex(22).toBeLessThanOrEqual(-0.7142);
	b /= 3;
	Expect(b.getf32()).withIndex(23).toBeGreaterThanOrEqual(-0.2381);
	Expect(b.getf32()).withIndex(24).toBeLessThanOrEqual(-0.2380);
	a = a + 2;
	Expect(a.getf32()).withIndex(25).toEqual(5.5);
	a = a + 2.5f;
	Expect(a.getf32()).withIndex(26).toEqual(8);
	a = a - 2;
	Expect(a.getf32()).withIndex(27).toEqual(6);
	a = a * 2;
	Expect(a.getf32()).withIndex(28).toEqual(12);
	a = a * 2.43;
	Expect(a.getf32()).withIndex(29).toBeGreaterThanOrEqual(29.159);
	Expect(a.getf32()).withIndex(30).toBeLessThanOrEqual(29.161);
	a = 4.25;
	a += 2;
	Expect(a.getf32()).withIndex(31).toEqual(6.25);
	a -= 2;
	Expect(a.getf32()).withIndex(32).toEqual(4.25);
	a *= 2;
	Expect(a.getf32()).withIndex(33).toEqual(8.5);
	a = 12;
	a *= 2.43;
	Expect(a.getf32()).withIndex(34).toBeGreaterThanOrEqual(29.159);
	Expect(a.getf32()).withIndex(35).toBeLessThanOrEqual(29.161);
	a = 2.5f;
	Expect(a.getf32()).withIndex(36).toEqual(2.5);
	a = 2.5;
	Expect(a.getf32()).withIndex(37).toEqual(2.5);
	Expect(a == 2).withIndex(38).toEqual(false);
	a = 2;
	Expect(a == 2).withIndex(39).toEqual(true);
	a = 2.5;
	b = 2.6;
	Expect(a == b).withIndex(40).toEqual(false);
	b = 2.5;
	Expect(a == b).withIndex(41).toEqual(true);
	b = 2.6;
	Expect(a < b).withIndex(42).toEqual(true);
	Expect(b < a).withIndex(43).toEqual(false);
	Expect(a > b).withIndex(44).toEqual(false);
	Expect(b > a).withIndex(45).toEqual(true);
	Expect(a <= b).withIndex(46).toEqual(true);
	Expect(b <= a).withIndex(47).toEqual(false);
	Expect(a >= b).withIndex(48).toEqual(false);
	Expect(b >= a).withIndex(49).toEqual(true);
	b = 2.5;
	Expect(a < b).withIndex(50).toEqual(false);
	Expect(b < a).withIndex(51).toEqual(false);
	Expect(a > b).withIndex(52).toEqual(false);
	Expect(b > a).withIndex(53).toEqual(false);
	Expect(a <= b).withIndex(54).toEqual(true);
	Expect(b <= a).withIndex(55).toEqual(true);
	Expect(a >= b).withIndex(56).toEqual(true);
	Expect(b >= a).withIndex(57).toEqual(true);
	a = 2;
	b = 2.5;
	b %= a;
	Expect(b.getf32()).withIndex(58).toEqual(0.5);
	b = 2.5;
	b = b >> 3;
	Expect(b.getf32()).withIndex(59).toEqual(0.3125);
	b = b << 3;
	Expect(b.getf32()).withIndex(60).toEqual(2.5);
	b = -b;
	Expect(b.getf32()).withIndex(61).toEqual(-2.5);
	Expect(b.sign()).withIndex(62).toEqual(-1);
	b = 2;
	Expect(b.sign()).withIndex(63).toEqual(1);
	b = 0;
	Expect(b.sign()).withIndex(64).toEqual(1);
	Expect(b.abs().getf32()).withIndex(65).toEqual(0);
	b = -2.5;
	Expect(b.abs().getf32()).withIndex(66).toEqual(2.5);
	b = 2.5;
	Expect(b.abs().getf32()).withIndex(67).toEqual(2.5);
	return ExpectBase::printResults(true, "FixedPoint");
}

void runUnitTests() {
	bool testsFailed = false;
	// ExpectBase::enableSilent();
	sleep_ms(5000);
	do {
		testsFailed = testRingBuffer() || testsFailed;
		testsFailed = testFixedPoint() || testsFailed;
		if (testsFailed) {
			Serial.println("Unit tests failed, rerun to see results.");
			ExpectBase::enableSilent(false);
			sleep_ms(10000);
		}
	} while (testsFailed);
}