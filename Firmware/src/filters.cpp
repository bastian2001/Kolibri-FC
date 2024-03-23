#include "filters.h"

PT1::PT1(fix32 cutoffFreq, u32 sampleFreq) : sampleFreq(sampleFreq) {
	fix32 omega = FIX_2PI * cutoffFreq / sampleFreq;
	alpha       = omega / (omega + 1);
}

PT1::PT1(fix32 alpha) : alpha(alpha) {}

void PT1::updateCutoffFreq(fix32 cutoffFreq) {
	fix32 omega = FIX_2PI * cutoffFreq / sampleFreq;
	alpha       = omega / (omega + 1);
}
void PT1::updateAlpha(fix32 alpha) { this->alpha = alpha; }