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
