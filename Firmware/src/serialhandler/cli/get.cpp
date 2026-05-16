/**
 * @file get.cpp
 * @brief Implementation of the get command
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

bool vectorHasString(const std::vector<string> &vec, const string &str) {
	for (const auto &s : vec) {
		if (s == str) {
			return true;
		}
	}
	return false;
}

void initGet() {
	Command *cmd = new Command("get", "Get the value of a setting or multiple settings");
	size_t maxNameLen = 0;
	for (auto setting : settingsList) {
		size_t idLen = strlen(setting->id);
		if (idLen > maxNameLen) {
			maxNameLen = idLen;
		}
	}
	cmd->addStringArg("name", 'n', false, true, maxNameLen, "Name of the setting to get (can be partial name). Use '*' to print all settings");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string response = "";
		string &name = std::get<string>(args["name"].value);
		if (name == "*") {
			name = "";
		}
		std::vector<string> ids;
		// start with exact match
		for (auto setting : settingsList) {
			if (setting->id == name) {
				ids.push_back(setting->id);
			}
		}
		// then any match that starts with the name
		for (auto setting : settingsList) {
			string id = setting->id;
			if (id.find(name) == 0 && !vectorHasString(ids, id)) {
				ids.push_back(id);
			}
		}
		// then any match that contains the name
		for (auto setting : settingsList) {
			string id = setting->id;
			if (id.find(name) != string::npos && !vectorHasString(ids, id)) {
				ids.push_back(id);
			}
		}

		if (ids.empty()) {
			response = CLI_COLOR_RED "Setting '" + string(name) + "' not found\n" CLI_COLOR_WHITE;
		} else {
			for (const auto &id : ids) {
				SettingBase *setting = getSetting(id.c_str());
				response += CLI_COLOR_CYAN + id + CLI_COLOR_WHITE ": " CLI_COLOR_GREEN + setting->toString(true) + "\n";
			}
		}
		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
