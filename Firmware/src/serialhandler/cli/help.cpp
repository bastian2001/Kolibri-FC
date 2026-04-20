#include "global.h"

void initHelp(){
	Command *cmd = new Command("help", "Print list of available commands");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string response = "Available commands:\n";
		for (Command *c : Command::cliCommands) {
			response += "  " + c->name + " - " + c->description + "\n";
		}
		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
