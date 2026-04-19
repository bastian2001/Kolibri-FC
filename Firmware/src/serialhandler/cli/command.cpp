#include "global.h"

std::vector<Command *> Command::cliCommands;
Command *Command::activeLoopCommand;

void initMan();

void initCli() {
	initMan();
}

void cliLoop() {
	if (Command::activeLoopCommand) {
		Command::activeLoopCommand->loop();
	}
}
