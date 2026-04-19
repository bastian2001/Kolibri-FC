#include "global.h"

void initMan() {
	Command *cmd = new Command("man", "Print manual/documentation for a command");
	cmd->addStringArg("command", 0, false, true, 20, "The command to print the manual for");
	cmd->setExecuteFunction([](string payload, Command *cmd) {
		
	});
	Command::cliCommands.push_back(cmd);
}
