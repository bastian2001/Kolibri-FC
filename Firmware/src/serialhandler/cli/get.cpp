#include "global.h"

bool vectorHasString(const std::vector<string> &vec, const string &str) {
	for (const auto &s : vec) {
		if (s == str) {
			return true;
		}
	}
	return false;
}

void initGet() {
	Command *cmd = new Command("get", "Get the value of a setting or multiple settings");
	size_t maxNameLen = 0;
	for (auto setting : settingsList) {
		size_t idLen = strlen(setting->id);
		if (idLen > maxNameLen) {
			maxNameLen = idLen;
		}
	}
	cmd->addStringArg("name", 'n', false, true, maxNameLen, "Name of the setting to get (can be partial name). Use '*' to print all settings");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		string response = "";
		string &name = std::get<string>(args["name"].value);
		if (name == "*") {
			name = "";
		}
		std::vector<string> ids;
		// start with exact match
		for (auto setting : settingsList) {
			if (setting->id == name) {
				ids.push_back(setting->id);
			}
		}
		// then any match that starts with the name
		for (auto setting : settingsList) {
			string id = setting->id;
			if (id.find(name) == 0 && !vectorHasString(ids, id)) {
				ids.push_back(id);
			}
		}
		// then any match that contains the name
		for (auto setting : settingsList) {
			string id = setting->id;
			if (id.find(name) != string::npos && !vectorHasString(ids, id)) {
				ids.push_back(id);
			}
		}

		if (ids.empty()) {
			response = "Setting '" + string(name) + "' not found\n";
		} else {
			for (const auto &id : ids) {
				SettingBase *setting = getSetting(id.c_str());
				response += id + ": " + setting->toString(true) + "\n";
			}
		}
		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
