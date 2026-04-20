#include "global.h"

void initGyroCalibration() {
	Command *cmd = new Command("gyro_calibration", "Start gyro calibration or get current calibration data");
	cmd->addStringArg("action", 'a', false, true, 5, "Action to perform: 'start' or 'get'");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string action = std::get<string>(args["action"].value);
		if (action == "start") {
			cmd->print("Calibrating gyro...");
			startGyroCalibration();
		} else if (action == "get") {
			char str[100];
			i16 cal[3];
			getGyroCalibration(cal);
			snprintf(str, sizeof(str), "Gyro calibration data: %d %d %d", cal[0], cal[1], cal[2]);
			cmd->print(str);
		} else {
			cmd->print("Invalid usage. Use 'gyro_calibration start' or 'gyro_calibration get'");
		}
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
