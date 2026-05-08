#include "global.h"

void initPrint() {
	Command *cmd = new Command("print", "Print the contents of the settings file");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		settingsFile.seek(0);
		string line;
		while (settingsFile.available()) {
			line = string(settingsFile.readStringUntil('\n').c_str()) + "\n";
			int pos = 0;
			if (!line.empty() && line[0] == ';') {
				line = CLI_COLOR_GREY + line + CLI_COLOR_WHITE;
			} else if (!line.empty() && (pos = line.find('=')) != string::npos) {
				string name = line.substr(0, pos);
				string value = line.substr(pos + 1);
				line = CLI_COLOR_CYAN + name + CLI_COLOR_WHITE "=" CLI_COLOR_GREEN + value;
			}
			cmd->print(line.c_str());
		}
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
