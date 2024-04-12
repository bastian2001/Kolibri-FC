#pragma once

#include <Arduino.h>

#define MAX_RTTTL_NOTES 256
#define MAX_RTTTL_TEXT_LENGTH 1024
#define SPEAKER_SIZE_POWER 10

/// @brief Initializes the speaker
void initSpeaker();

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