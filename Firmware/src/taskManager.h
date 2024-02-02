#include "typedefs.h"
#include <Arduino.h>

typedef struct task {
	u32 runCounter;
	u32 minDuration;
	u32 maxDuration;
	u32 frequency;
	u32 avgDuration;
	u32 errorCount;
	u32 lastError;
	u32 totalDuration;
	u32 debugInfo;
	u32 maxGap;
} FCTask;
extern FCTask tasks[32];

enum Tasks {
	TASK_BARO,
	TASK_SPEAKER,
	TASK_BLACKBOX,
	TASK_ELRS,
	TASK_MODES,
	TASK_ADC,
	TASK_SERIAL,
	TASK_CONFIGURATOR,
	TASK_GPS,
	TASK_GYROREAD,
	TASK_IMU,
	TASK_PID_MOTORS,
	TASK_OSD,
	TASK_TASKMANAGER,
	TASK_LOOP1,
	TASK_LOOP0,
};

void initTaskManager();
void taskManagerLoop();
