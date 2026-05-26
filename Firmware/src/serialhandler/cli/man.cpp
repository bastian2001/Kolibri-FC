/**
 * @file man.cpp
 * @brief Implementation of the man command (actual printing happens in cli.cpp)
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

static Command cmd("man", "Print manual/documentation for a command");
static std::vector<SelectionOption> cmds;

void initMan() {
	cmd.setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		if (!cmd->getSerial()) return false;
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
		command->printMan(cmd->getSerial());
		return false;
	});
	Command::cliCommands.push_back(&cmd);
}

void finishInitMan() {
	for (Command *c : Command::cliCommands) {
		cmds.push_back({c->name, ""});
	}
	cmd.addSelectionArg("command", 'c', false, true, &cmds, "The command to print the manual for", "", false);
}
