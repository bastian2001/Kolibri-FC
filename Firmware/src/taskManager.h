/**
 * @file taskManager.h
 * @brief Task manager functions, timekeeping shorthands and variables
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

#include "typedefs.h"
#include <Arduino.h>

#define TASK_START(taskname) elapsedMicros taskTimer##taskname = 0;
#if __ARM_FEATURE_SIMD32
#define TASK_END(taskname)                               \
	u32 duration##taskname = taskTimer##taskname;        \
	tasks[taskname].runCounter++;                        \
	tasks[taskname].totalDuration += duration##taskname; \
	tasks[taskname].minMaxDuration = minmax16x2(duration##taskname << 16 | duration##taskname, tasks[taskname].minMaxDuration);
#else
#define TASK_END(taskname)                                \
	u32 duration##taskname = taskTimer##taskname;         \
	tasks[taskname].runCounter++;                         \
	tasks[taskname].totalDuration += duration##taskname;  \
	if (duration##taskname < tasks[taskname].minDuration) \
		tasks[taskname].minDuration = duration##taskname; \
	if (duration##taskname > tasks[taskname].maxDuration) \
		tasks[taskname].maxDuration = duration##taskname;
#endif

enum Tasks {
	TASK_LOOP0,
	TASK_SPEAKER,
	TASK_BARO,
	TASK_BARO_CHECK,
	TASK_BARO_READ,
	TASK_BARO_EVAL,
	TASK_BLACKBOX_WRITE,
	TASK_ELRS,
	TASK_ELRS_MSG,
	TASK_MODES,
	TASK_ADC,
	TASK_SERIAL,
	TASK_CONFIGURATOR,
	TASK_GPS,
	TASK_GPS_MSG,
	TASK_MAG,
	TASK_MAG_CHECK,
	TASK_MAG_READ,
	TASK_MAG_EVAL,
	TASK_OSD,
	TASK_ANALOG_OSD,
	TASK_VTX,
	TASK_TASKMANAGER,
	TASK_LOOP1,
	TASK_GYROREAD,
	TASK_IMU,
	TASK_IMU_GYRO,
	TASK_IMU_ACCEL1,
	TASK_IMU_ACCEL2,
	TASK_IMU_ANGLE,
	TASK_IMU_SPEEDS,
	TASK_CONTROL,
	TASK_CONTROL_1,
	TASK_CONTROL_2,
	TASK_CONTROL_3,
	TASK_CONTROL_4,
	TASK_CONTROL_5,
	TASK_CONTROL_6,
	TASK_CONTROL_7,
	TASK_CONTROL_8,
	TASK_ESC_RPM,
	TASK_PID,
	TASK_BLACKBOX,
	TASK_LENGTH
};

typedef struct task {
	u32 runCounter; // incremented every time the task is run, reset every second
	union {
		struct {
			i16 maxDuration; // minimum duration of the task
			i16 minDuration; // maximum duration of the task
		};
		int16x2_t minMaxDuration; // 32 bit shorthand for min/max duration (min high, max low bytes) for minmax16x2 and MSP
	};
	u32 frequency; // how often the task is run in the last second
	u32 lastTotalDuration; // the total duration that this task took during the last second
	u32 errorCount; // how often the task has thrown an error since boot
	u32 lastError; // last error code (different for each task)
	u32 totalDuration; // total duration the task has taken since the reset every second
	u32 debugInfo; // debug info (different for each task)
	u32 maxGap; // maximum gap between two runs of the task (from end to start)
} FCTask;
extern volatile FCTask tasks[TASK_LENGTH]; // holds all the task stats

/// @brief resets all task stats
void initTaskManager();

/// @brief checks if a second has passed and updates the task stats
void taskManagerLoop();
