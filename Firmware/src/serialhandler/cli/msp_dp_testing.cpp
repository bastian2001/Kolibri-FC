/**
 * @file msp_dp_testing.cpp
 * @brief Command to test some MSP DP HD stuff
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
std::vector<SelectionOption> options = {
	{"sd", "SD canvas (30x16)"},
	{"hd", "123"},
	{"sdinhd", "123"},
	{"inavwtf", "123"},
};

void initMspDp() {
	Command *cmd = new Command("dp", "MSP DP testing command");
	cmd->addSelectionArg("action", 0, false, true, &options);
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		const auto &actionArg = args["action"];
		string text = std::get<string>(actionArg.value);
		MspMsgSetup setup = {
			.serial = *serials[3],
			.fn = MspFn::MSP_DISPLAYPORT,
			.type = MspMsgType::RESPONSE,
			.version = MspVersion::V1,
		};
		u8 data[3] = {5, 0, 0};
		if (text == "sd") {
			data[2] = 0;
		} else if (text == "hd") {
			data[2] = 1;
		} else if (text == "sdinhd") {
			data[2] = 2;
		} else if (text == "inavwtf") {
			data[2] = 3;
		} else {
			cmd->print("Invalid action");
			return false;
		}
		sendMsp(setup, (char *)data, 3);
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
