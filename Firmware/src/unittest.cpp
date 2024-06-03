#include "unittest.h"
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

void runUnitTests() {
	bool testsFailed = false;
	// ExpectBase::enableSilent();
	do {
		testsFailed = testRingBuffer() || testsFailed;
		if (testsFailed) {
			Serial.println("Unit tests failed, rerun to see results.");
			ExpectBase::enableSilent(false);
			sleep_ms(10000);
		}
	} while (testsFailed);
}