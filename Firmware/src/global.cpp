#include "global.h"

bool parseInt(const char *str, i64 &value) {
	if (!str) return false;

	errno = 0;
	char *end = nullptr;

	i64 v = std::strtoll(str, &end, 10);
	if (end == str) return false;
	if (*end != '\0') return false;
	if (errno == ERANGE && (value == LONG_LONG_MAX || value == LONG_LONG_MIN)) return false;

	value = v;
	return true;
}
bool parseFloat(const char *str, f64 &value) {
	if (!str) return false;

	errno = 0;
	char *end = nullptr;

	f64 v = std::strtod(str, &end);
	if (end == str) return false;
	if (*end != '\0') return false;
	if (errno == ERANGE && (value == HUGE_VAL || value == -HUGE_VAL || value == 0.0)) return false;

	value = v;
	return true;
}

std::optional<ExpressLRS> elrs;

NeoPixelConnect p(PIN_LEDS, 9, PIO_LED);
std::string uavName = "";

bool core1_separate_stack = true; // 8k+8k stack instead of 4k+4k
