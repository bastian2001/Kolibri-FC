/**
 * @file speaker.h
 * @brief Function declarations for controlling the speaker
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

#pragma once

#include <Arduino.h>
#include <string>

#define MAX_RTTTL_NOTES 256
#define MAX_RTTTL_TEXT_LENGTH 1024
#define SPEAKER_SIZE_POWER 10 // 2^x samples, thus the buffer is 2^x * 2 bytes

/// @brief Initializes the speaker
void initSpeaker();

/// @brief Plays the start sound, either from a WAV file or a fallback RTTTL string
void playStartSound();

/**
 * @brief Play a WAV file from the SD card
 *
 * @param filename Path to the WAV file
 * @return true if the file was found
 * @return false if the file was not found
 */
bool playWav(const char *filename);

/// @brief Called periodically to update the speaker with new PWM or WAV data
void speakerLoop();

/**
 * @brief Start a beeping sound
 *
 * @details The sound will be played for tOnMs, then off for tOffMs, and repeated until duration ms have passed
 *
 * @param frequency Frequency in Hz
 * @param duration Duration in ms (65535 for infinite)
 * @param tOnMs Time the sound is on in ms
 * @param tOffMs Time the sound is off in ms
 */
void makeSound(u16 frequency, u16 duration, u16 tOnMs, u16 tOffMs);

/// @brief Stop the current sound
void stopSound();

/**
 * @brief Play a sweep sound
 *
 * @details sweep from startFrequency to endFrequency over tOnMs, then stop for tOffMs, repeat until duration ms have passed
 *
 * @param startFrequency start frequency of the sweep
 * @param endFrequency end frequency of the sweep
 * @param duration Duration in ms (65535 for infinite)
 * @param tOnMs Time the sound is on in ms
 * @param tOffMs Time the sound is off in ms
 */
void makeSweepSound(u16 startFrequency, u16 endFrequency, u16 duration, u16 tOnMs, u16 tOffMs);

/// @brief Play a sound in RTTTL format
/// @param song RTTTL formatted song string
void makeRtttlSound(const char *song);
