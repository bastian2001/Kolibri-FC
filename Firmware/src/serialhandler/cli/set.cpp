#include "global.h"

static std::vector<SelectionOption> options;

void initSet() {
	Command *cmd = new Command("set", "Set the value of a setting");

	for (const auto &setting : settingsList) {
		options.push_back({setting->id, ""});
	}
	cmd->addSelectionArg("name", 'n', false, true, &options, "Name of the setting to set", "", false);
	cmd->addStringArg("value", 'v', false, true, 256, "Value to set the setting to");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string response = "";
		string &name = std::get<string>(args["name"].value);
		string &value = std::get<string>(args["value"].value);
		SettingBase *setting = getSetting(name.c_str());
		if (setting == &dummySetting) {
			response = "Setting '" + string(name) + "' not found\n";
		} else {
			if (setting->setDataFromString(value, true)) {
				response = "Setting updated: " + string(name);
				setting->updateSettingInFile();
			} else {
				response = "Invalid value for setting: " + string(name);
			}
		}
		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
