#include "global.h"

void initMan() {
	Command *cmd = new Command("man", "Print manual/documentation for a command");
	cmd->addStringArg("command", 'c', false, true, 20, "The command to print the manual for");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		const auto &commandArg = args["command"];
		string commandName = std::get<string>(commandArg.value);
		Command *command = nullptr;
		for (Command *c : Command::cliCommands) {
			if (c->nameMatches(commandName)) {
				command = c;
				break;
			}
		}
		if (!command) {
			cmd->print("Command not found\n");
			return false;
		}
		command->printMan(cmd->getSerialNum());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
