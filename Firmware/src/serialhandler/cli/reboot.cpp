/**
 * @file reboot.cpp
 * @brief Implementation of the reboot command
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

static std::vector<SelectionOption> options = {{"fc", "Reboot normally"}, {"bootloader", "Reboot to bootloader"}};

void initReboot() {
	Command *cmd = new Command("reboot", "Reboot the FC");
	cmd->addSelectionArg("mode", 'm', true, false, &options, "Reboot mode", "fc");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string mode = std::get<string>(args["mode"].value);
		if (mode == "fc") {
			cmd->print("Rebooting...");
			if (cmd->getSerial()) cmd->getSerial()->flush();
			sleep_ms(100);
			rp2040.reboot();
		} else if (mode == "bootloader") {
			cmd->print("Rebooting to bootloader...");
			if (cmd->getSerial()) cmd->getSerial()->flush();
			sleep_ms(100);
			rp2040.rebootToBootloader();
		}
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
