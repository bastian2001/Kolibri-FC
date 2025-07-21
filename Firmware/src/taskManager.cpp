#include "global.h"
__attribute__((__aligned__(4))) volatile FCTask tasks[32];

elapsedMillis taskManagerTimer;

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
	START_TASK(TASK_TASKMANAGER);
	if (taskManagerTimer >= 1000) {
		taskManagerTimer = 0;
		for (int i = 0; i < 32; i++) {
			tasks[i].frequency = tasks[i].runCounter;
			if (tasks[i].runCounter > 0) {
				tasks[i].lastTotalDuration = tasks[i].totalDuration;
				tasks[i].totalDuration = 0;
				tasks[i].runCounter = 0;
			}
		}
	}
	END_TASK(TASK_TASKMANAGER);
}
