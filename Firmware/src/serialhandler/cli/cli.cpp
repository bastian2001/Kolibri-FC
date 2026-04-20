#include "global.h"

std::vector<Command *> Command::cliCommands;
Command *Command::activeLoopCommand;

void initEcho();
void initGet();
void initGyroCalibration();
void initHelp();
void initMan();
void initPrint();
void initReboot();
void initReset();
void initSave();
void initSerialStats();
void initSet();
void initStatus();

void initCli() {
	initEcho();
	initGet();
	initGyroCalibration();
	initHelp();
	initMan();
	initPrint();
	initReboot();
	initReset();
	initSave();
	initSerialStats();
	initSet();
	initStatus();
}

void cliLoop() {
	if (Command::activeLoopCommand) {
		Command::activeLoopCommand->loop();
	}
}
