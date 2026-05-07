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
