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
extern volatile FCTask tasks[32]; // holds all the task stats

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
	TASK_TASKMANAGER,
	TASK_LOOP1,
	TASK_GYROREAD,
	TASK_IMU,
	TASK_IMU_GYRO,
	TASK_IMU_ACCEL,
	TASK_IMU_ANGLE,
	TASK_IMU_SPEEDS,
	TASK_ESC_RPM,
	TASK_PID,
	TASK_BLACKBOX
};

/// @brief resets all task stats
void initTaskManager();

/// @brief checks if a second has passed and updates the task stats
void taskManagerLoop();
