#include "global.h"

ExpressLRS *ELRS;
u32 DECODE_U4(const u8 *buf) {
	u32 result;
	memcpy(&result, buf, 4); // memcpy needed because of 4-byte-alignment
	return result;
}
i32 DECODE_I4(const u8 *buf) {
	i32 result;
	memcpy(&result, buf, 4);
	return result;
}
f32 DECODE_R4(const u8 *buf) {
	f32 result;
	memcpy(&result, buf, 4);
	return result;
}
i64 DECODE_I8(const u8 *buf) {
	i64 result;
	memcpy(&result, buf, 8);
	return result;
}
f64 DECODE_R8(const u8 *buf) {
	f64 result;
	memcpy(&result, buf, 8);
	return result;
}

volatile u32 __uninitialized_ram(crashInfo)[256];