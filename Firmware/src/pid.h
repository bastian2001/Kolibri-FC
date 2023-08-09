#pragma once
#include <Arduino.h>

extern uint16_t bmiData[6];
extern uint16_t *gyroData;
extern uint16_t *accelData;

void pidLoop();