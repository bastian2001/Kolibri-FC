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
extern volatile FCTask tasks[32];

enum Tasks {
	TASK_BAROREAD,
	TASK_BAROEVAL,
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
	TASK_IMU_GYRO,
	TASK_IMU_ACCEL,
	TASK_IMU_ANGLE,
	TASK_ESC_RPM
};

void initTaskManager();
void taskManagerLoop();
