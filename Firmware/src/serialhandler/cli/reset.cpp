/**
 * @file reset.cpp
 * @brief Implementation of the reset command
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

void initReset() {
	Command *cmd = new Command("reset", "Reset the FC to factory defaults (will erase settings file)");
	// cmd->addFlagArg("confirm", 0, "Type '--confirm' to confirm resetting the FC");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("Proceed with factory reset? (y/N) ");
		return true;
	});
	cmd->setInputFunction([](string input, Command *cmd) {
		cmd->print((input + "\n").c_str());
		if (input == "y" || input == "Y") {
			cmd->print("Resetting settings...");
			if (cmd->getSerial()) cmd->getSerial()->flush();
			sleep_ms(100);
			closeSettingsFile();
			LittleFS.remove("/settings.txt");
			rp2040.reboot();
		} else {
			cmd->print("Factory reset aborted");
		}
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
