/**
 * @file print.cpp
 * @brief Implementation of the print command
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

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
