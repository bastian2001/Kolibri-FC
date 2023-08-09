#pragma once
#include <Arduino.h>

extern PIO escPio;
extern uint8_t escSm;
extern uint32_t motorPacket[2];

void initESCs();

// 11 bit value without checksum and telemetry bit
void sendThrottles(uint16_t throttles[4]);