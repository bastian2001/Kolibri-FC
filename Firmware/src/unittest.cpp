#include "unittest.h"
#include "ringbuffer.h"
#include <fixedPointInt.h>

u32 ExpectBase::failed = false;
u32 ExpectBase::succeeded = false;
bool ExpectBase::silentLogging = false;
template <>
const char *Fmt<i32>::f = "%16d";
template <>
const char *Fmt<u32>::f = "%16u";
template <>
const char *Fmt<f32>::f = "%16.2f";
template <>
const char *Fmt<f64>::f = "%16.2lf";
template <>
const char *Fmt<i16>::f = "%16d";
template <>
const char *Fmt<u16>::f = "%16u";
template <>
const char *Fmt<i8>::f = "%16d";
template <>
const char *Fmt<u8>::f = "%16u";
template <>
const char *Fmt<bool>::f = "%16d";
template <>
const char *Fmt<i64>::f = "%16lld";
template <>
const char *Fmt<u64>::f = "%16llu";
template <>
const char *Fmt<fix32>::f = "%16X";
template <>
const char *Fmt<fix64>::f = "%16llX";

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
	fix32 a1;
	a1.setRaw(0b101 << 15);
	Expect(a1.raw).withIndex(0).toEqual(0b101 << 15);
	a1 = 2;
	Expect(a1.raw).withIndex(1).toEqual(2 << 16);
	a1 = 2U;
	Expect(a1.raw).withIndex(2).toEqual(2 << 16);
	a1 = 2.5f;
	Expect(a1.raw).withIndex(3).toEqual(0b101 << 15);
	a1 = 2.5;
	Expect(a1.raw).withIndex(4).toEqual(0b101 << 15);
	a1 = fix64(2);
	Expect(a1.raw).withIndex(5).toEqual(2 << 16);
	Expect(a1.setRaw(0b101 << 15).raw).withIndex(6).toEqual(0b101 << 15);
	Expect(a1.raw).withIndex(7).toEqual(0b101 << 15);
	Expect(a1.getf32()).withIndex(8).toEqual(2.5f);
	Expect(a1.getf64()).withIndex(9).toEqual(2.5);
	Expect(a1.geti32()).withIndex(10).toEqual(2);
	Expect(a1.getu32()).withIndex(11).toEqual(2);
	a1 = 2.9;
	Expect(a1.geti32()).withIndex(12).toEqual(2);
	a1 = 2.5;
	Expect((a1.getfix64()).raw).withIndex(13).toEqual(0b101LL << 31);
	fix32 a2 = 2.75;
	Expect(a1 + a2).withIndex(14).toEqual(5.25);
	Expect(a1 - a2).withIndex(15).toEqual(-0.25);
	Expect(a1 * a2).withIndex(16).toEqual(6.875);
	a2 = 3.125;
	Expect(a2 / a1).withIndex(17).toEqual(1.25);
	Expect(a2 % a1).withIndex(18).toEqual(0.625);
	a2 = 2.75;
	a1 += a2;
	Expect(a1).withIndex(19).toEqual(5.25);
	a1 -= a2;
	Expect(a1).withIndex(20).toEqual(2.5);
	a1 *= a2;
	Expect(a1).withIndex(21).toEqual(6.875);
	a1 /= a2;
	Expect(a1).withIndex(22).toEqual(2.5);
	a2 = -2.75;
	Expect(a1 * a2).withIndex(23).toEqual(-6.875);
	Expect(a2 * a1).withIndex(24).toEqual(-6.875);
	a2 = 2.75;
	a2 %= a1;
	Expect(a2).withIndex(25).toEqual(0.25);
	Expect(a1 == a2).withIndex(26).toEqual(false);
	Expect(a1 != a2).withIndex(27).toEqual(true);
	a2 = 2.5;
	Expect(a1 == a2).withIndex(28).toEqual(true);
	Expect(a1 != a2).withIndex(29).toEqual(false);
	Expect(a1 > a2).withIndex(30).toEqual(false);
	Expect(a1 >= a2).withIndex(31).toEqual(true);
	Expect(a1 < a2).withIndex(32).toEqual(false);
	Expect(a1 <= a2).withIndex(33).toEqual(true);
	a1 = 2.6;
	Expect(a1 > a2).withIndex(34).toEqual(true);
	Expect(a1 >= a2).withIndex(35).toEqual(true);
	Expect(a1 < a2).withIndex(36).toEqual(false);
	Expect(a1 <= a2).withIndex(37).toEqual(false);
	a1 = 2.5;
	a1 = a1 + 2;
	Expect(a1).withIndex(38).toEqual(4.5);
	a1 = a1 - 2;
	Expect(a1).withIndex(39).toEqual(2.5);
	a1 = a1 * 2;
	Expect(a1).withIndex(40).toEqual(5);
	a1 = a1 / 2;
	Expect(a1).withIndex(41).toEqual(2.5);
	a1 += 2;
	Expect(a1).withIndex(42).toEqual(4.5);
	a1 -= 2;
	Expect(a1).withIndex(43).toEqual(2.5);
	a1 *= 2;
	Expect(a1).withIndex(44).toEqual(5);
	a1 /= 2;
	Expect(a1).withIndex(45).toEqual(2.5);
	a1 = a1 + 2U;
	Expect(a1).withIndex(46).toEqual(4.5);
	a1 = a1 - 2U;
	Expect(a1).withIndex(47).toEqual(2.5);
	a1 = a1 * 2U;
	Expect(a1).withIndex(48).toEqual(5);
	a1 = a1 / 2U;
	Expect(a1).withIndex(49).toEqual(2.5);
	a1 += 2U;
	Expect(a1).withIndex(50).toEqual(4.5);
	a1 -= 2U;
	Expect(a1).withIndex(51).toEqual(2.5);
	a1 *= 2U;
	Expect(a1).withIndex(52).toEqual(5);
	a1 /= 2U;
	Expect(a1).withIndex(53).toEqual(2.5);
	a1 -= 15U;
	Expect(a1).withIndex(54).toEqual(-12.5);
	a1 += 2;
	Expect(a1).withIndex(55).toEqual(-10.5);
	a1 += 11;
	Expect(a1).withIndex(56).toEqual(0.5);
	a1 += -11;
	Expect(a1).withIndex(57).toEqual(-10.5);
	a1 = 2.5;
	a1 = a1 + 2.5f;
	Expect(a1).withIndex(58).toEqual(5);
	a1 = a1 - 2.5f;
	Expect(a1).withIndex(59).toEqual(2.5);
	a1 = a1 * 2.5f;
	Expect(a1).withIndex(60).toEqual(6.25);
	a1 = a1 / 2.5f;
	Expect(a1).withIndex(61).toEqual(2.5);
	a1 += 2.5f;
	Expect(a1).withIndex(62).toEqual(5);
	a1 -= 2.5f;
	Expect(a1).withIndex(63).toEqual(2.5);
	a1 *= 2.5f;
	Expect(a1).withIndex(64).toEqual(6.25);
	a1 /= 2.5f;
	Expect(a1).withIndex(65).toEqual(2.5);
	a1 = a1 * -2.5f;
	Expect(a1).withIndex(66).toEqual(-6.25);
	Expect(a1 * -1).withIndex(67).toEqual(6.25);
	Expect(a1 / -1).withIndex(68).toEqual(6.25);
	a1 = 2.5f;
	a1 /= 0.125f;
	Expect(a1).withIndex(69).toEqual(20);
	a1 = 2.5f;
	a1 = a1 >> 1;
	Expect(a1).withIndex(70).toEqual(1.25);
	a1 = a1 << 2;
	Expect(a1).withIndex(71).toEqual(5);
	a1 = -a1;
	Expect(a1).withIndex(72).toEqual(-5);
	Expect(a1.sign()).withIndex(73).toEqual(-1);
	a1 = 2;
	Expect(a1.sign()).withIndex(74).toEqual(1);
	a1 = 0;
	Expect(a1.sign()).withIndex(75).toEqual(1);
	Expect(a1.abs()).withIndex(76).toEqual(0);
	a1 = -2;
	Expect(a1.abs()).withIndex(77).toEqual(2);
	a1 = 2;
	Expect(a1.abs()).withIndex(78).toEqual(2);
	Expect(a1 + 2.5).withIndex(79).toEqual(4.5);
	Expect(a1 - 2.5).withIndex(80).toEqual(-0.5);
	Expect(a1 * 2.5).withIndex(81).toEqual(5);
	Expect(a1 / -0.125).withIndex(82).toEqual(-16);
	Expect(a1 += 2.5).withIndex(83).toEqual(4.5);
	Expect(a1 -= 2.5).withIndex(84).toEqual(2);
	Expect(a1 *= 2.5).withIndex(85).toEqual(5);
	Expect(a1 /= 2.5).withIndex(86).toEqual(2);
	a1 = 2.5;
	Expect(a1 > 2.49).withIndex(87).toEqual(true);
	Expect(a1 >= 2.49).withIndex(88).toEqual(true);
	Expect(a1 < 2.51).withIndex(89).toEqual(true);
	Expect(a1 <= 2.51).withIndex(90).toEqual(true);
	Expect(a1 >= 2.5).withIndex(91).toEqual(true);
	Expect(a1 <= 2.5).withIndex(92).toEqual(true);
	Expect(a1 > 2.5).withIndex(93).toEqual(false);
	Expect(a1 < 2.5).withIndex(94).toEqual(false);
	Expect(a1 >= 2.59).withIndex(95).toEqual(false);
	Expect(a1 <= 2.49).withIndex(96).toEqual(false);

	fix64 b1 = fix64().setRaw(0b111LL << 31);
	Expect(a1 + b1).withIndex(97).toEqual(6);
	Expect(a1 + -b1).withIndex(98).toEqual(-1);
	Expect(a1 - b1).withIndex(99).toEqual(-1);
	Expect(a1 - -b1).withIndex(100).toEqual(6);
	b1 = 10000;
	Expect(a1 * b1).withIndex(101).toEqual(25000);
	Expect(a1 * -b1).withIndex(102).toEqual(-25000);
	Expect(a1 / b1).withIndex(105).toBeGreaterThan(0.00024);
	Expect(a1 / b1).withIndex(106).toBeLessThan(0.00026);
	Expect(a1 += b1).withIndex(107).toEqual(10002.5);
	Expect(a1 -= b1).withIndex(108).toEqual(2.5);
	Expect(a1 -= b1).withIndex(109).toEqual(-9997.5);
	a1 = 2.5;
	Expect(a1 *= b1).withIndex(110).toEqual(25000);
	Expect(a1 /= b1).withIndex(111).toEqual(2.5);
	b1 = 2.6;
	Expect(a1 > b1).withIndex(112).toEqual(false);
	Expect(a1 >= b1).withIndex(113).toEqual(false);
	Expect(a1 < b1).withIndex(114).toEqual(true);
	Expect(a1 <= b1).withIndex(115).toEqual(true);
	b1 = 2.5;
	Expect(a1 > b1).withIndex(116).toEqual(false);
	Expect(a1 >= b1).withIndex(117).toEqual(true);
	Expect(a1 < b1).withIndex(118).toEqual(false);
	Expect(a1 <= b1).withIndex(119).toEqual(true);
	b1 = 2.4;
	Expect(a1 > b1).withIndex(120).toEqual(true);
	Expect(a1 >= b1).withIndex(121).toEqual(true);
	Expect(a1 < b1).withIndex(122).toEqual(false);
	Expect(a1 <= b1).withIndex(123).toEqual(false);

	b1 = 2;
	Expect(b1.raw).withIndex(124).toEqual(0b10LL << 32);
	b1 = 2.5f;
	Expect(b1.raw).withIndex(125).toEqual(0b101LL << 31);
	b1 = 2.5;
	Expect(b1.raw).withIndex(126).toEqual(0b101LL << 31);
	Expect(b1.setRaw(0b111LL << 31).raw).withIndex(127).toEqual(0b111LL << 31);
	a1 = 42.69;
	Expect(a1.getfix64().getfix32()).withIndex(128).toEqual(a1);
	Expect(b1.getfix32()).withIndex(129).toEqual(3.5);
	fix64 b2 = 42069.125;
	b1 = 420690;
	Expect(b1 + b2).withIndex(130).toEqual(462759.125);
	Expect(b1 + -b2).withIndex(131).toEqual(378620.875);
	Expect(-b1 + b2).withIndex(132).toEqual(-378620.875);
	Expect(-b1 + -b2).withIndex(133).toEqual(-462759.125);
	Expect(b1 - b2).withIndex(134).toEqual(378620.875);
	Expect(-b1 - b2).withIndex(135).toEqual(-462759.125);
	Expect(-b1 - -b2).withIndex(136).toEqual(-378620.875);
	b1 = 50972.00000762939453125; // 50972 + 1/131072 or 1/2^17, uses 16+17 bits of the f64 (53 max)
	Expect(b1 * b2).withIndex(137).toEqual(2144347439.82096195220947265625);
	Expect(b1 * -b2).withIndex(138).toEqual(-2144347439.82096195220947265625);
	Expect(-b1 * b2).withIndex(139).toEqual(-2144347439.82096195220947265625);
	Expect(-b1 * -b2).withIndex(140).toEqual(2144347439.82096195220947265625);
	b2 = 2.5;
	Expect(b2.getf32()).withIndex(141).toEqual(2.5f);
	Expect(b2.getf64()).withIndex(142).toEqual(2.5);
	Expect(b2.geti32()).withIndex(143).toEqual(2);
	Expect(b2.getu32()).withIndex(144).toEqual(2);
	a1 = 2.5;
	Expect(b1 + a1).withIndex(145).toEqual(50974.50000762939453125);
	Expect(b1 + -a1).withIndex(146).toEqual(50969.50000762939453125);
	Expect(-b1 + a1).withIndex(147).toEqual(-50969.50000762939453125);
	Expect(-b1 + -a1).withIndex(148).toEqual(-50974.50000762939453125);
	Expect(b1 - a1).withIndex(149).toEqual(50969.50000762939453125);
	Expect(b1 * a1).withIndex(150).toEqual(127430.000019073486328125);
	Expect(b1 * -a1).withIndex(150).toEqual(-127430.000019073486328125);
	Expect(-b1 * a1).withIndex(150).toEqual(-127430.000019073486328125);
	Expect(-b1 * -a1).withIndex(150).toEqual(127430.000019073486328125);
	Expect(b1 / a1).withIndex(151).toEqual(20388.8000030517578125);
	Expect(b1 / fix32(0.0078125)).withIndex(151).toEqual(6524416.0009765625);
	Expect(b1 + 2).withIndex(152).toEqual(50974.00000762939453125);
	Expect(b1 - 2).withIndex(153).toEqual(50970.00000762939453125);
	Expect(b1 * 2).withIndex(154).toEqual(101944.0000152587890625);
	Expect(b1 / 2).withIndex(155).toEqual(25486.000003814697265625);
	Expect(b1 + 2.5f).withIndex(156).toEqual(50974.50000762939453125);
	Expect(b1 - 2.5f).withIndex(157).toEqual(50969.50000762939453125);
	Expect(b1 * 2.5f).withIndex(158).toEqual(127430.000019073486328125);
	Expect(b1 / 2.5f).withIndex(159).toEqual(20388.8000030517578125);
	b1 = 15.25;
	Expect(b1 >> 1).withIndex(160).toEqual(7.625);
	Expect(b1 << 1).withIndex(161).toEqual(30.5);
	Expect(-b1).withIndex(162).toEqual(-15.25);
	Expect(b1.sign()).withIndex(163).toEqual(1);
	b1 = -15.25;
	Expect(b1.sign()).withIndex(164).toEqual(-1);
	b1 = 0;
	Expect(b1.sign()).withIndex(165).toEqual(1);
	b1 = 15.25;
	Expect(b1.abs()).withIndex(166).toEqual(15.25);
	b1 = -15.25;
	Expect(b1.abs()).withIndex(167).toEqual(15.25);
	b1 = 15.25;
	b2 = 15.25;
	Expect(b1 == b2).withIndex(168).toEqual(true);
	Expect(b1 != b2).withIndex(169).toEqual(false);
	Expect(b1 > b2).withIndex(170).toEqual(false);
	Expect(b1 < b2).withIndex(171).toEqual(false);
	b2 = 15.26;
	Expect(b1 == b2).withIndex(172).toEqual(false);
	Expect(b1 != b2).withIndex(173).toEqual(true);
	Expect(b1 > b2).withIndex(174).toEqual(false);
	Expect(b1 < b2).withIndex(175).toEqual(true);
	b2 = 15.24;
	Expect(b1 == b2).withIndex(176).toEqual(false);
	Expect(b1 != b2).withIndex(177).toEqual(true);
	Expect(b1 > b2).withIndex(178).toEqual(true);
	Expect(b1 < b2).withIndex(179).toEqual(false);
	a1 = 0.03;
	b1 = 1000;
	b1 = b1 / a1;
	Expect(b1).withIndex(180).toBeGreaterThan(33330);
	Expect(b1).withIndex(181).toBeLessThan(33336);

	return ExpectBase::printResults(true, "FixedPoint");
}

void runUnitTests() {
	bool testsFailed = false;
	CHECK_TYPE_SIZE(f32, 4);
	CHECK_TYPE_SIZE(f64, 8);
	CHECK_TYPE_SIZE(i64, 8);
	CHECK_TYPE_SIZE(u64, 8);
	CHECK_TYPE_SIZE(i32, 4);
	CHECK_TYPE_SIZE(u32, 4);
	CHECK_TYPE_SIZE(i16, 2);
	CHECK_TYPE_SIZE(u16, 2);
	CHECK_TYPE_SIZE(i8, 1);
	CHECK_TYPE_SIZE(u8, 1);
	ExpectBase::enableSilent();
	bool firstRun = true;
	do {
		testsFailed = testRingBuffer() || testsFailed;
		testsFailed = testFixedPoint() || testsFailed;
		if (testsFailed) {
			Serial.println("Unit tests failed, rerun to see results.");
			ExpectBase::enableSilent(false);
			if (firstRun) {
				firstRun = false;
				sleep_ms(10000);
			} else {
				sleep_ms(120000);
			}
		}
	} while (testsFailed);
}
