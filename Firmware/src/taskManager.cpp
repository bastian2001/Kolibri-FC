/**
 * @file taskManager.cpp
 * @brief Task manager implementation for tracking task execution times and frequencies
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

#include "global.h"
__attribute__((__aligned__(4))) volatile FCTask tasks[TASK_LENGTH];

static elapsedMicros taskManagerTimer;

void resetTasks() {
	for (int i = 0; i < TASK_LENGTH; i++) {
		tasks[i].runCounter = 0;
		tasks[i].minMaxDuration = 0x7FFF0000;
		tasks[i].frequency = 0;
		tasks[i].lastTotalDuration = 0;
		tasks[i].totalDuration = 0;
	}
}

void initTaskManager() {
	resetTasks();
}
void taskManagerLoop() {
	TASK_START(TASK_TASKMANAGER);
	if (taskManagerTimer >= 1000000) {
		taskManagerTimer = 0;
		for (int i = 0; i < TASK_LENGTH; i++) {
			tasks[i].frequency = tasks[i].runCounter;
			tasks[i].lastTotalDuration = tasks[i].totalDuration;
			tasks[i].totalDuration = 0;
			tasks[i].runCounter = 0;
		}
	}
	TASK_END(TASK_TASKMANAGER);
}
