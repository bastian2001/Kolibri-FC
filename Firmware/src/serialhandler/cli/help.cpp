#include "global.h"

void initHelp() {
	Command *cmd = new Command("help", "Print list of available commands");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string response = "Available commands:";
		for (Command *c : Command::cliCommands) {
			response += "\n\t" + c->name + " - " + c->description;
		}
		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
