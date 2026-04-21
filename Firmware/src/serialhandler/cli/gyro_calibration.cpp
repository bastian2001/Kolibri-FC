#include "global.h"
static std::vector<SelectionOption> options = {{"start", "Start the gyro calibration"}, {"get", "Print the current gyro calibration data"}};
static elapsedMicros commandTimer = 0;
static u8 spinner = 0;
static const char spinnerChars[4] = {'|', '/', '-', '\\'};

void initGyroCalibration() {
	Command *cmd = new Command("gyro_calibration", "Start gyro calibration or get current calibration data");
	cmd->addSelectionArg("action", 'a', false, true, &options, "Action to perform");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string action = std::get<string>(args["action"].value);
		if (action == "start") {
			cmd->print("Calibrating gyro... |");
			startGyroCalibration();
			commandTimer = 0;
			spinner = 0;
			return true; // Return true to indicate that the command is still running
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
	cmd->setLoopFunction([](Command *cmd) {
		if (commandTimer > 100000) {
			char str[50];
			if (++spinner >= 4) spinner = 0;
			snprintf(str, sizeof(str), "\rCalibrating gyro... %c", spinnerChars[spinner]);
			cmd->print(str);
			commandTimer = 0;
		}
		if (!(gyroReadyFlags & (1 << 1))) {
			cmd->print(CLI_COLOR_GREEN "\rGyro calibration complete!" CLI_COLOR_WHITE);
			return false; // Return false to indicate that the command is complete
		}
		return true; // Return true to keep the command running
	});
	Command::cliCommands.push_back(cmd);
}
