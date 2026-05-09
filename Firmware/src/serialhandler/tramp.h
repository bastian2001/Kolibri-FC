/**
 * @file tramp.h
 * @brief Interface for Tramp VTX handler functions
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Arduino.h"
#include "drivers/halfduplexUart.h"
#include "ringbuffer.h"
#include "typedefs.h"

extern RingBuffer<u8> trampRxBuffer;

void setTrampSerial(KoliSerial *ptr);

void trampInit();

void trampLoop();

u8 sendTrampUpdateMsg(char *buf);

u8 sendTrampConfigMsg(char *buf);

void setTrampConfig(const char *buf);

void applyTrampConfig();
