/**
 * @file clear_et_al.cpp
 * @brief Implementation of the clear, screen and exit commands
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

void initClear() {
	Command *cmd = new Command("clear", "Clear the screen");
	cmd->addAlias("cls");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("\v"); // VT character clears the screen in our terminal emulator
		return false;
	});
	Command::cliCommands.push_back(cmd);
}

void initScreen() {
	Command *cmd = new Command("screen", "Start a new shell session (clears the screen), returning to the previous session on exit");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("\x0f"); // SI character starts a new session in our terminal emulator
		return false;
	});
	Command::cliCommands.push_back(cmd);
}

void initExit() {
	Command *cmd = new Command("exit", "Exit the current shell session, returning to the previous session if screen command was used");
	cmd->addAlias("quit");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("\x0e"); // SO character exits the current session in our terminal emulator
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
