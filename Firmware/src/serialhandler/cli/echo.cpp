/**
 * @file echo.cpp
 * @brief Implementation of the echo command
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

void initEcho() {
	Command *cmd = new Command("echo", "Echo back the provided text");
	cmd->addStringArg("text", 't', false, true, 400, "The text to echo back. Use quotes to include spaces, e.g. echo \"Hello world\", or escape spaces with a backslash, e.g. echo Hello\\ world.");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		const auto &textArg = args["text"];
		string text = std::get<string>(textArg.value);
		cmd->print(text.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
