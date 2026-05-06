#include "global.h"

void initSave() {
	Command *cmd = new Command("save", "Save current settings to file");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		closeSettingsFile();
		openSettingsFile();
		cmd->print(CLI_COLOR_GREEN "Settings saved" CLI_COLOR_WHITE);
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
