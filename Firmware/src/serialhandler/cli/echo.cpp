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
