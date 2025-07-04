#pragma once

#include "elapsedMillis.h"
#include "serialhandler/msp.h"
#include "typedefs.h"
#include <Arduino.h>
#define LOG_ELRS_RAW (1 << 0) // 6 bytes
#define LOG_ROLL_SETPOINT (1 << 4) // 2 bytes
#define LOG_PITCH_SETPOINT (1 << 5) // 2 bytes
#define LOG_THROTTLE_SETPOINT (1 << 6) // 2 bytes
#define LOG_YAW_SETPOINT (1 << 7) // 2 bytes
#define LOG_ROLL_GYRO_RAW (1 << 8) // 2 bytes
#define LOG_PITCH_GYRO_RAW (1 << 9) // 2 bytes
// 20 bytes total
#define LOG_YAW_GYRO_RAW (1 << 10) // 2 bytes
#define LOG_ROLL_PID_P (1 << 11) // 2 bytes
#define LOG_ROLL_PID_I (1 << 12) // 2 bytes
#define LOG_ROLL_PID_D (1 << 13) // 2 bytes
#define LOG_ROLL_PID_FF (1 << 14) // 2 bytes
#define LOG_ROLL_PID_S (1 << 15) // 2 bytes
#define LOG_PITCH_PID_P (1 << 16) // 2 bytes
#define LOG_PITCH_PID_I (1 << 17) // 2 bytes
#define LOG_PITCH_PID_D (1 << 18) // 2 bytes
#define LOG_PITCH_PID_FF (1 << 19) // 2 bytes
// 40 bytes total
#define LOG_PITCH_PID_S (1 << 20) // 2 bytes
#define LOG_YAW_PID_P (1 << 21) // 2 bytes
#define LOG_YAW_PID_I (1 << 22) // 2 bytes
#define LOG_YAW_PID_D (1 << 23) // 2 bytes
#define LOG_YAW_PID_FF (1 << 24) // 2 bytes
#define LOG_YAW_PID_S (1 << 25) // 2 bytes
#define LOG_MOTOR_OUTPUTS (1 << 26) // 6 bytes
#define LOG_FRAMETIME (1 << 27) // 2 bytes
// 60 bytes total
#define LOG_ALTITUDE (1 << 29) // 2 bytes

#define LOG_VVEL (1 << 30) // 2 bytes
#define LOG_GPS (1U << 31) // 2 bytes
#define LOG_ATT_ROLL (1LL << 32) // 2 bytes
#define LOG_ATT_PITCH (1LL << 33) // 2 bytes
#define LOG_ATT_YAW (1LL << 34) // 2 bytes
#define LOG_MOTOR_RPM (1LL << 35) // 6 bytes
#define LOG_ACCEL_RAW (1LL << 36) // 6 bytes
// 85 bytes total
#define LOG_ACCEL_FILTERED (1LL << 37) // 6 bytes
#define LOG_VERTICAL_ACCEL (1LL << 38) // 2 bytes
#define LOG_VVEL_SETPOINT (1LL << 39) // 2 bytes
#define LOG_MAG_HEADING (1LL << 40) // 2 bytes
#define LOG_COMBINED_HEADING (1LL << 41) // 2 bytes
#define LOG_HVEL (1LL << 42) // 4 bytes
#define LOG_BARO (1LL << 43) // 3 bytes
#define LOG_DEBUG_1 (1LL << 44) // 4 bytes
#define LOG_DEBUG_2 (1LL << 45) // 4 bytes
#define LOG_DEBUG_3 (1LL << 46) // 2 bytes
#define LOG_DEBUG_4 (1LL << 47) // 2 bytes
// 118 bytes total

#define LOG_HEAD_MAGIC 0
#define LOG_HEAD_BB_VERSION 4
#define LOG_HEAD_TIMESTAMP 7
#define LOG_HEAD_PID_FREQ 11
#define LOG_HEAD_LOOP_DIV 12
#define LOG_HEAD_GYRO_ACCEL_RANGE 13
#define LOG_HEAD_RATE_FACTORS 14
#define LOG_HEAD_PID_GAINS 74
#define LOG_HEAD_LOGGED_FIELDS 134
#define LOG_HEAD_MOTOR_POLES 142
#define LOG_HEAD_DURATION 143
#define LOG_HEAD_LENGTH 256

#define BB_FRAME_NORMAL 0 // normal frame, i.e. gyro, setpoints, pid, etc.
#define BB_FRAME_FLIGHTMODE 1 // flight mode change
#define BB_FRAME_HIGHLIGHT 2 // highlight frame, user pressed a button to highlight this frame
#define BB_FRAME_GPS 3 // GPS frame, i.e. PVT message
#define BB_FRAME_RC 4 // RC frame, ELRS channels

extern u64 bbFlags; // 64 bits of flags for the blackbox (LOG_ macros)
extern volatile bool bbLogging, fsReady; // Blackbox state
extern u8 bbFreqDivider; // Blackbox frequency divider (compared to PID loop)
extern u32 bbDebug1, bbDebug2;
extern u16 bbDebug3, bbDebug4;
extern SdFs sdCard; // SD card filesystem

/// @brief Set up SD card and create /blackbox folder
void initBlackbox();

/// @brief Start a logging file
void startLogging();

/// @brief Stop logging and close the file
void endLogging();

/**
 * @brief Delete all blackbox files
 *
 * @return true if all files were deleted
 * @return false if any file could not be deleted
 */
bool clearBlackbox();

/// @brief Write a single frame to the blackbox file
void writeSingleFrame();

/**
 * @brief Print a log file to the configurator using MspFn::BB_FILE_DOWNLOAD
 *
 * @details Chunk Size is 1024 bytes
 *
 * @param serialNum serial number of the device
 * @param mspVer MSP version to use
 * @param logNum log number to print
 * @param singleChunk chunk number to print, -1 for all
 */
void printLogBin(u8 serialNum, MspVersion mspVer, u16 logNum, i32 singleChunk);

/**
 * @brief Print the file number, the file size and the chunk size
 *
 * @param serialNum serial number of the device
 * @param mspVer MSP version to use
 * @param logNum log number to print
 */
void printFileInit(u8 serialNum, MspVersion mspVer, u16 logNum);

/// @brief Writes the prepared blackbox frames to the SD card
void blackboxLoop();
