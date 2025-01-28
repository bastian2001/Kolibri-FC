#include "filters.h"

PT1::PT1(fix32 cutoffFreq, u32 sampleFreq) : sampleFreq(sampleFreq) {
	if (sampleFreq <= 0 || cutoffFreq <= 0) alpha = 1;
	fix32 omega = FIX_2PI * cutoffFreq / sampleFreq;
	alpha = omega / (omega + 1);
}

PT1::PT1(fix32 alpha) : alpha(alpha) {}

void PT1::updateCutoffFreq(fix32 cutoffFreq) {
	if (sampleFreq <= 0 || cutoffFreq <= 0) return;
	fix32 omega = FIX_2PI * cutoffFreq / sampleFreq;
	alpha = omega / (omega + 1);
}
void PT1::updateAlpha(fix32 alpha) { this->alpha = alpha; }

void PT1::setRolloverParams(fix32 lowerBound, fix32 upperBound) {
	this->lowerBound = lowerBound;
	this->upperBound = upperBound;
	boundDiff = upperBound - lowerBound;
}

fix32 PT1::rollover() {
	if (y < lowerBound) {
		y += boundDiff;
	} else if (y >= upperBound) {
		y -= boundDiff;
	}
	return y;
}