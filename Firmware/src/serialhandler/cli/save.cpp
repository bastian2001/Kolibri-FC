/**
 * @file save.cpp
 * @brief Implementation of the save command
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
