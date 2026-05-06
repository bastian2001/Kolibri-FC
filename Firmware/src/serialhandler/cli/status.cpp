#include "global.h"

void initStatus(){
	Command *cmd = new Command("status", "Get current status information about the FC");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string response = "Firmware: " FIRMWARE_NAME " " FIRMWARE_VERSION_STRING "\n";
		response += "UAV Name: " + uavName + "\n";
		response += "Uptime: " + std::to_string(millis()) + " ms\n";
		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
