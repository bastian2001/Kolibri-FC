#include "global.h"

std::optional<ExpressLRS> elrs;

NeoPixelConnect p(PIN_LEDS, 9, PIO_LED);
std::string uavName = "";

bool core1_separate_stack = true; // 8k+8k stack instead of 4k+4k
