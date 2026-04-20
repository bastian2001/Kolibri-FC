#include "global.h"

static Command cmd("man", "Print manual/documentation for a command");
static std::vector<SelectionOption> cmds;

void initMan() {
	cmd.setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
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
	Command::cliCommands.push_back(&cmd);
}

void finishInitMan() {
	for (Command *c : Command::cliCommands) {
		cmds.push_back({c->name, c->description});
	}
	cmd.addSelectionArg("command", 'c', false, true, &cmds, "The command to print the manual for");
}
