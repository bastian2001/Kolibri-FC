#include "FS.h"
#include "elapsedMillis.h"
#include <Arduino.h>
#include "typedefs.h"
#define LOG_ROLL_ELRS_RAW 1 << 0
#define LOG_PITCH_ELRS_RAW 1 << 1
#define LOG_THROTTLE_ELRS_RAW 1 << 2
#define LOG_YAW_ELRS_RAW 1 << 3
#define LOG_ROLL_SETPOINT 1 << 4
#define LOG_PITCH_SETPOINT 1 << 5
#define LOG_THROTTLE_SETPOINT 1 << 6
#define LOG_YAW_SETPOINT 1 << 7
#define LOG_ROLL_GYRO_RAW 1 << 8
#define LOG_PITCH_GYRO_RAW 1 << 9
#define LOG_YAW_GYRO_RAW 1 << 10
#define LOG_ROLL_PID_P 1 << 11
#define LOG_ROLL_PID_I 1 << 12
#define LOG_ROLL_PID_D 1 << 13
#define LOG_ROLL_PID_FF 1 << 14
#define LOG_ROLL_PID_S 1 << 15
#define LOG_PITCH_PID_P 1 << 16
#define LOG_PITCH_PID_I 1 << 17
#define LOG_PITCH_PID_D 1 << 18
#define LOG_PITCH_PID_FF 1 << 19
#define LOG_PITCH_PID_S 1 << 20
#define LOG_YAW_PID_P 1 << 21
#define LOG_YAW_PID_I 1 << 22
#define LOG_YAW_PID_D 1 << 23
#define LOG_YAW_PID_FF 1 << 24
#define LOG_YAW_PID_S 1 << 25
#define LOG_MOTOR_OUTPUTS 1 << 26
#define LOG_ALTITUDE 1 << 27
#define LOG_FRAMETIME 1 << 28

#define LOG_HEAD_MAGIC 0
#define LOG_HEAD_BB_VERSION 4
#define LOG_HEAD_TIMESTAMP 7
#define LOG_HEAD_PID_FREQ 11
#define LOG_HEAD_LOOP_DIV 12
#define LOG_HEAD_GYRO_ACCEL_RANGE 13
#define LOG_HEAD_RATE_FACTORS 14
#define LOG_HEAD_PID_GAINS 74
#define LOG_HEAD_LOGGED_FIELDS 158

extern u64 bbFlags;
extern bool bbLogging;
extern FSInfo64 fs_info;
extern elapsedMillis infoAge;
extern u8 bbFreqDivider;

void initBlackbox();

void startLogging();

void endLogging();

bool clearBlackbox();

void writeSingleFrame();

void printLogBin(u8 logNum, i16 chunkNum = -1);
void printLogBinRaw(u8 logNum);

void blackboxLoop();