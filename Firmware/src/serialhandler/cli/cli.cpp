#include "global.h"

std::vector<Command *> Command::cliCommands;
Command *Command::activeLoopCommand;

void initClear();
void initEcho();
void initExit();
void initGet();
void initGyroCalibration();
void initHelp();
void initMan();
void initPrint();
void initReboot();
void initReset();
void initSave();
void initScreen();
void initSerialStats();
void initSet();
void initStatus();

void finishInitMan();

void initCli() {
	initClear();
	initEcho();
	initExit();
	initGet();
	initGyroCalibration();
	initHelp();
	initMan();
	initPrint();
	initReboot();
	initReset();
	initSave();
	initScreen();
	initSerialStats();
	initSet();
	initStatus();

	finishInitMan();
}

void cliLoop() {
	if (Command::activeLoopCommand) {
		Command::activeLoopCommand->loop();
	}
}
