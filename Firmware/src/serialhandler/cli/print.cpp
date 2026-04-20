#include "global.h"

void initPrint() {
	Command *cmd = new Command("print", "Print the contents of the settings file");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		settingsFile.seek(0);
		string line;
		while (settingsFile.available()) {
			line = string(settingsFile.readStringUntil('\n').c_str()) + "\n";
			cmd->print(line.c_str());
		}
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
