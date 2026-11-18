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

static u8 phase = 0;
static elapsedMillis timer = 0;
void initTest() {
	Command *cmd = new Command("test", "test spinner");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("\x0f"); // SI character starts a new session in our terminal emulator
		return true;
	});
	cmd->setAbortFunction([](Command *cmd) {
		cmd->print("\x0e");
	});
	cmd->setLoopFunction([](Command *cmd) {
		if (timer < 100) return true;
		timer = 0;
		string s = "\v";
		for (int i = 0; i < (cmd->getHeight() - 3) / 2; i++) {
			s += " \n";
		}
		u32 empty = (cmd->getWidth() - 3) / 2;
		if (phase > 2) s += " \n";
		if (phase > 3 && phase < 7) s += " \n";
		switch (phase) {
		case 0:
		case 6:
		case 7:
			break;
		case 1:
		case 5:
			empty++;
			break;
		case 2:
		case 3:
		case 4:
			empty += 2;
			break;
		}
		for (int i = 0; i < empty; i++) {
			s += " ";
		}
		phase++;
		if (phase == 8) phase = 0;
		s += "#";
		cmd->print(s.c_str());
		return true;
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

	initTest();
}
