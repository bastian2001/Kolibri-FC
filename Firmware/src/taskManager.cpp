#include "global.h"
__attribute__((__aligned__(4))) volatile FCTask tasks[32];

elapsedMicros taskManagerTimer;

void resetTasks() {
	for (int i = 0; i < 32; i++) {
		tasks[i].runCounter = 0;
		tasks[i].minDuration = 0xFFFFFFFF;
		tasks[i].maxDuration = 0;
		tasks[i].frequency = 0;
	}
}

void initTaskManager() {
	resetTasks();
}
void taskManagerLoop() {
	TASK_START(TASK_TASKMANAGER);
	if (taskManagerTimer >= 1000000) {
		taskManagerTimer = 0;
		for (int i = 0; i < 32; i++) {
			tasks[i].frequency = tasks[i].runCounter;
			tasks[i].lastTotalDuration = tasks[i].totalDuration;
			tasks[i].totalDuration = 0;
			tasks[i].runCounter = 0;
		}
	}
	TASK_END(TASK_TASKMANAGER);
}
