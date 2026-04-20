#include "global.h"

std::vector<Command *> Command::cliCommands;
Command *Command::activeLoopCommand;

void initEcho();
void initHelp();
void initMan();

void initCli() {
	initEcho();
	initHelp();
	initMan();
}

void cliLoop() {
	if (Command::activeLoopCommand) {
		Command::activeLoopCommand->loop();
	}
}
