#include "global.h"

void initSave() {
	Command *cmd = new Command("save", "Save current settings to file");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		closeSettingsFile();
		openSettingsFile();
		cmd->print("Settings saved");
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
