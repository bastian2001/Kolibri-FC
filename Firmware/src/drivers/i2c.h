#pragma once
#include "typedefs.h"
extern u8 i2c0blocker;

void startI2cRead(u8 addr, u8 len);

void startI2cWrite(u8 addr, u8 *buf, u8 len);

int checkI2cWriteDone();

int getI2cReadCount();

int getI2cReadData(u8 *buf, u8 len);
