#include "global.h"
// 	} else if (cmd == "reboot") {
// 		sendMsp(lastMspSerial, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, "Rebooting...", 13);
// 		if (serials[lastMspSerial]) serials[lastMspSerial]->flush();
// 		sleep_ms(100);
// 		rp2040.reboot();

void initReboot() {
	Command *cmd = new Command("reboot", "Reboot the FC");
	cmd->addStringArg("mode", 'm', true, false, 2, "Reboot mode: fc = normal reboot, bl = reboot to bootloader", "fc");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string mode = std::get<string>(args["mode"].value);
		if (mode == "fc") {
			cmd->print("Rebooting...");
			if (serials[cmd->getSerialNum()]) serials[cmd->getSerialNum()]->flush();
			sleep_ms(100);
			rp2040.reboot();
		} else if (mode == "bl") {
			cmd->print("Rebooting to bootloader...");
			if (serials[cmd->getSerialNum()]) serials[cmd->getSerialNum()]->flush();
			sleep_ms(100);
			rp2040.rebootToBootloader();
		} else {
			cmd->print(string("Invalid mode: " + mode + "").c_str());
		}
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
