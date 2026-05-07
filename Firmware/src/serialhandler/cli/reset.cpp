#include "global.h"

void initReset() {
	Command *cmd = new Command("reset", "Reset the FC to factory defaults (will erase settings file)");
	cmd->addFlagArg("confirm", 0, "Type '--confirm' to confirm resetting the FC");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		const auto &confirmationArg = args["confirm"];
		bool confirmation = std::get<bool>(confirmationArg.value);
		if (confirmation) {
			cmd->print("Resetting settings...");
			if (cmd->getSerial()) cmd->getSerial()->flush();
			sleep_ms(100);
			closeSettingsFile();
			LittleFS.remove("/settings.txt");
			rp2040.reboot();
		} else {
			cmd->print("If you are sure you want to reset the whole FC, use 'reset confirm'");
		}
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
