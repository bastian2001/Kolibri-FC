#pragma once
#include <Arduino.h>

extern int16_t bmiDataRaw[6];
extern int16_t *gyroDataRaw;
extern int16_t *accelDataRaw;
extern int32_t imuData[6];

void pidLoop();

void initPID();