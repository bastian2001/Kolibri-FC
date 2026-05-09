/**
 * @file serial_stats.cpp
 * @brief Implementation of the serial_stats command
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

void initSerialStats() {
	Command *cmd = new Command("serial_stats", "Get or reset statistics about the serial ports");
	cmd->addFlagArg("reset", 'r', "Reset the serial statistics after printing");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		const auto &resetArg = args["reset"];
		bool reset = std::get<bool>(resetArg.value);
		string response = "";

		f32 timeSinceReset = f32(KoliSerial::sinceReset) / 1000.f;
		response = "Statistics since the last reset (" + std::to_string((u32)timeSinceReset) + "ms ago)\n";
		for (int i = 0; i < SERIAL_COUNT; i++) {
			char line[128];
			if (!serials[i]) continue;
			KoliSerial &s = *serials[i];
			snprintf(line, 128, CLI_COLOR_CYAN "Serial %d" CLI_COLOR_MAGENTA " (%s)" CLI_COLOR_WHITE ":" CLI_COLOR_BLUE "     TX: %7d bytes (%.2fKB/s)" CLI_COLOR_WHITE "," CLI_COLOR_BLUE "     RX: %7d bytes (%.2fKB/s)" CLI_COLOR_WHITE ",     " CLI_COLOR_YELLOW, i, KoliSerial::serialTypeNames[(u8)s.serialType], s.totalTx, s.totalTx / timeSinceReset, s.totalRx, s.totalRx / timeSinceReset);
			response += line;
			bool firstFunction = true;
			for (int j = 0; j < SERIAL_FUNCTION_COUNT; j++) {
				if (s.functions & (1 << j)) {
					if (firstFunction)
						firstFunction = false;
					else
						response += ", ";
					response += serialFunctionNames[j];
				}
			}
			response += '\n';
		}

		if (reset) {
			for (auto &s : serials) {
				if (!s) continue;
				while (s->totalRx || s->totalTx) {
					// clearing may fail in case a byte is read/written atm
					s->totalRx = 0;
					s->totalTx = 0;
				}
			}
			KoliSerial::sinceReset = 0;
			response += CLI_COLOR_GREEN "\nSerial statistics reset" CLI_COLOR_WHITE;
		}

		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
