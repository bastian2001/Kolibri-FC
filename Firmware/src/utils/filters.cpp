#include "filters.h"
#include <cmath>

PT1::PT1(fix32 cutoffFreq, u32 sampleFreq) : sampleFreq(sampleFreq) {
	if (sampleFreq <= 0 || cutoffFreq <= 0) alpha = 1;
	fix32 omega = FIX_PI * 2 * cutoffFreq / sampleFreq;
	alpha = omega / (omega + 1);
}

PT1::PT1(fix32 alpha) : alpha(alpha) {}

void PT1::updateCutoffFreq(fix32 cutoffFreq) {
	if (sampleFreq <= 0 || cutoffFreq <= 0) return;
	fix32 omega = FIX_PI * 2 * cutoffFreq / sampleFreq;
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

void PT1::set(fix32 value) {
	y = value;
}

DualPT1::DualPT1(fix32 cutoffFreq, u32 sampleFreq) : sampleFreq(sampleFreq) {
	firstRatio = sqrtf(sampleFreq / cutoffFreq.getf32());
	pt1a = PT1(sampleFreq / firstRatio, sampleFreq);
	pt1b = PT1(cutoffFreq, sampleFreq / firstRatio);
	pt1c = PT1(sampleFreq / firstRatio / 8, sampleFreq);
}

PT2::PT2(fix32 cutoffFreq, u32 sampleFreq) : sampleFreq(sampleFreq) {
	fix32 omega = FIX_PI * 2 * 1.5537f * cutoffFreq / sampleFreq;
	alpha = omega / (omega + 1);
}

PT2::PT2(fix32 alpha) : alpha(alpha) {}

void PT2::updateCutoffFreq(fix32 cutoffFreq) {
	fix32 omega = FIX_PI * 2 * 1.5537f * cutoffFreq / sampleFreq;
	alpha = omega / (omega + 1);
}

void PT2::updateAlpha(fix32 alpha) { this->alpha = alpha; }

void PT2::setRolloverParams(fix32 lowerBound, fix32 upperBound) {
	this->lowerBound = lowerBound;
	this->upperBound = upperBound;
	boundDiff = upperBound - lowerBound;
}

fix32 PT2::rollover() {
	if (y < lowerBound) {
		y += boundDiff;
	} else if (y >= upperBound) {
		y -= boundDiff;
	}
	return y;
}

void PT2::set(fix32 value) {
	y = value;
	y1 = value;
}

PT3::PT3(fix32 cutoffFreq, u32 sampleFreq) : sampleFreq(sampleFreq) {
	fix32 omega = FIX_PI * 2 * 1.9615f * cutoffFreq / sampleFreq;
	alpha = omega / (omega + 1);
}

PT3::PT3(fix32 alpha) : alpha(alpha) {}

void PT3::updateCutoffFreq(fix32 cutoffFreq) {
	fix32 omega = FIX_PI * 2 * 1.9615f * cutoffFreq / sampleFreq;
	alpha = omega / (omega + 1);
}

void PT3::updateAlpha(fix32 alpha) { this->alpha = alpha; }

void PT3::setRolloverParams(fix32 lowerBound, fix32 upperBound) {
	this->lowerBound = lowerBound;
	this->upperBound = upperBound;
	boundDiff = upperBound - lowerBound;
}

fix32 PT3::rollover() {
	if (y < lowerBound) {
		y += boundDiff;
	} else if (y >= upperBound) {
		y -= boundDiff;
	}
	return y;
}

void PT3::set(fix32 value) {
	y = value;
	y1 = value;
	y2 = value;
}
