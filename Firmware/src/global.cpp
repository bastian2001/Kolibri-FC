#include "global.h"

ExpressLRS *ELRS;
uint32_t DECODE_U4(const uint8_t *buf) {
	uint32_t result;
	memcpy(&result, buf, 4); // memcpy needed because of 4-byte-alignment
	return result;
}
int32_t DECODE_I4(const uint8_t *buf) {
	int32_t result;
	memcpy(&result, buf, 4);
	return result;
}
float DECODE_R4(const uint8_t *buf) {
	float result;
	memcpy(&result, buf, 4);
	return result;
}
int64_t DECODE_I8(const uint8_t *buf) {
	int64_t result;
	memcpy(&result, buf, 8);
	return result;
}
double DECODE_R8(const uint8_t *buf) {
	double result;
	memcpy(&result, buf, 8);
	return result;
}

uint32_t __uninitialized_ram(crashInfo)[256];