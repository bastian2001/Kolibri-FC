#include "global.h"
__attribute__((__aligned__(4))) volatile FCTask tasks[32];

elapsedMillis taskManagerTimer;

void resetTasks() {
	for (int i = 0; i < 32; i++) {
		tasks[i].runCounter = 0;
		tasks[i].minDuration = 0xFFFFFFFF;
		tasks[i].maxDuration = 0;
		tasks[i].frequency = 0;
		tasks[i].avgDuration = 0;
	}
}

void initTaskManager() {
	resetTasks();
}
void taskManagerLoop() {
	elapsedMicros taskTimer = 0;
	tasks[TASK_TASKMANAGER].runCounter++;
	if (taskManagerTimer >= 1000) {
		taskManagerTimer = 0;
		for (int i = 0; i < 32; i++) {
			tasks[i].frequency = tasks[i].runCounter;
			if (tasks[i].runCounter > 0) {
				tasks[i].avgDuration = tasks[i].totalDuration / tasks[i].runCounter;
				tasks[i].totalDuration = 0;
				tasks[i].runCounter = 0;
			}
		}
		tasks[TASK_MAG].debugInfo = magRunCounter;
		magRunCounter = 0;
	}
	u32 duration = taskTimer;
	tasks[TASK_TASKMANAGER].totalDuration += duration;
	if (duration < tasks[TASK_TASKMANAGER].minDuration) {
		tasks[TASK_TASKMANAGER].minDuration = duration;
	}
	if (duration > tasks[TASK_TASKMANAGER].maxDuration) {
		tasks[TASK_TASKMANAGER].maxDuration = duration;
	}
}
