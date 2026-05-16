/**
 * @file set.cpp
 * @brief Implementation of the set command
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

static std::vector<SelectionOption> options;

void initSet() {
	Command *cmd = new Command("set", "Set the value of a setting");

	for (const auto &setting : settingsList) {
		options.push_back({setting->id, ""});
	}
	cmd->addSelectionArg("name", 'n', false, true, &options, "Name of the setting to set", "", false);
	cmd->addStringArg("value", 'v', false, true, 256, "Value to set the setting to");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string response = "";
		string &name = std::get<string>(args["name"].value);
		string &value = std::get<string>(args["value"].value);
		SettingBase *setting = getSetting(name.c_str());
		if (setting == &dummySetting) {
			response = CLI_COLOR_RED "Setting '" + string(name) + "' not found\n" CLI_COLOR_WHITE;
		} else {
			if (setting->setDataFromString(value, true)) {
				response = "Setting updated: " CLI_COLOR_CYAN + string(name) + CLI_COLOR_WHITE;
				setting->updateSettingInFile();
			} else {
				response = CLI_COLOR_RED "Invalid value for setting: " + string(name) + CLI_COLOR_WHITE;
			}
		}
		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
