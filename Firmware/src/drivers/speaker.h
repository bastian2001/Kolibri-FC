#pragma once

#include <Arduino.h>

#define MAX_RTTTL_NOTES 256
#define MAX_RTTTL_TEXT_LENGTH 1024

void initSpeaker();

void startBootupSound();

void speakerLoop();

void makeSound(uint16_t frequency, uint16_t duration, uint16_t tOnMs, uint16_t tOffMs);

void stopSound();

// sweep from startFrequency to endFrequency over tOnMs, then stop for tOffMs, repeat for duration
void makeSweepSound(uint16_t startFrequency, uint16_t endFrequency, uint16_t duration, uint16_t tOnMs, uint16_t tOffMs);

void makeRtttlSound(const char *song);