#include "Arduino.h"
#include "typedefs.h"
#pragma once

using std::string;

enum class ArgType {
	STRING,
	INT,
	FLOAT,
	FLAG,
};
typedef struct commandArg {
	string name;
	char shorthand = 0;
	bool optional = false;
	bool anonymous = false; // if true, the argument is not identified by name in the command string, but only by its position. In this case, the name is only used for documentation/manual purposes.
	string description = "";
	string defaultValue = "";
	ArgType type = ArgType::INT;
	union {
		struct {
			int min;
			int max;
		} intLimits;
		struct {
			float min;
			float max;
		} floatLimits;
		int maxStrLen; // for string arguments, the maximum length of the string (not including the null terminator)
	};
} CommandArg;
typedef struct runtimeArg {
	bool provided = false;
	union {
		string stringValue;
		int intValue;
		float floatValue;
		bool flagValue;
	};
} RuntimeArg;

class Command {
public:
	Command() = delete;
	Command(const Command &) = delete;
	Command(string name, string description = "") : name(name), description(description) {}
	void addAlias(string alias) {
		aliases.push_back(alias);
	}
	void addStringArg(string name, char shorthand = 0, bool optional = false, bool anonymous = false, int maxLen = 16, string description = "", string defaultValue = "") {
		if (anonymous) optional = false;
		CommandArg arg;
		arg.name = name;
		arg.shorthand = shorthand;
		arg.optional = optional;
		arg.anonymous = anonymous;
		arg.description = description;
		arg.defaultValue = defaultValue;
		arg.type = ArgType::STRING;
		arg.maxStrLen = maxLen;
		args.push_back(arg);
	}
	void addIntArg(string name, char shorthand = 0, bool optional = false, bool anonymous = false, int min = 0, int max = 255, string description = "", string defaultValue = "") {
		if (anonymous) optional = false;
		CommandArg arg;
		arg.name = name;
		arg.shorthand = shorthand;
		arg.optional = optional;
		arg.anonymous = anonymous;
		arg.description = description;
		arg.defaultValue = defaultValue;
		arg.type = ArgType::INT;
		arg.intLimits.min = min;
		arg.intLimits.max = max;
		args.push_back(arg);
	}
	void addFloatArg(string name, char shorthand = 0, bool optional = false, bool anonymous = false, float min = 0, float max = 1, string description = "", string defaultValue = "") {
		if (anonymous) optional = false;
		CommandArg arg;
		arg.name = name;
		arg.shorthand = shorthand;
		arg.optional = optional;
		arg.anonymous = anonymous;
		arg.description = description;
		arg.defaultValue = defaultValue;
		arg.type = ArgType::FLOAT;
		arg.floatLimits.min = min;
		arg.floatLimits.max = max;
		args.push_back(arg);
	}
	void addFlagArg(string name, char shorthand = 0, string description = "") {
		CommandArg arg;
		arg.name = name;
		arg.shorthand = shorthand;
		arg.optional = true; // flags are always optional
		arg.anonymous = false; // flags are identified by their name in the command string
		arg.description = description;
		arg.defaultValue = "";
		arg.type = ArgType::FLAG;
		args.push_back(arg);
	}

	void execute(string payload, u8 serialNum) {
		// if (!serial) return;
		// this->serial = serial;
		this->serialNum = serialNum;
		if (executeFunction) {
			if (executeFunction(payload, this)) {
				if (activeLoopCommand) activeLoopCommand->abort();
				activeLoopCommand = this;
			} else {
				const char *buf = "\n>> ";
				sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, buf, strlen(buf));
			}
		}
	};
	void abort() {
		if (abortFunction) abortFunction(this);
	};
	void loop() {
		if (loopFunction) {
			if (!loopFunction(this)) {
				activeLoopCommand = nullptr;
				const char *buf = "\n>> ";
				sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, buf, strlen(buf));
			}
		}
	};
	void input(string input) {
		if (inputFunction) inputFunction(input, this);
	};
	void printMan(u8 serialNum) {
		// if (!serial) return;
		string man = "NAME\n  " + name + " - " + description;
		if (!aliases.empty()) {
			man += "\n\nALIASES\n  ";
			for (const auto &alias : aliases) {
				man += alias + " ";
			}
		}
		man += "\n\nUSAGE\n  " + name;
		bool hasOptionalArgs = false;
		for (const auto &arg : args) {
			if (arg.optional) {
				hasOptionalArgs = true;
				continue;
			}
			if (arg.anonymous) {
				man += " " + arg.name;
			} else {
				if (arg.shorthand)
					man += " -" + string(1, arg.shorthand) + " " + arg.name;
				else
					man += " --" + arg.name + " " + arg.name;
			}
		}
		if (hasOptionalArgs) {
			man += " ...args";
		}
		man += "\n\nARGUMENTS\n";
		for (const auto &arg : args) {
			string argStr;
			if (!arg.anonymous) {
				if (arg.shorthand)
					argStr += "   -" + string(1, arg.shorthand);
				else
					argStr += "     ";
				argStr += ", --" + arg.name;
				for (int i = 10 - arg.name.length(); i > 0; i--) {
					argStr += " ";
				}
			} else {
				argStr += "       <" + arg.name + ">";
				for (int i = 10 - arg.name.length(); i > 0; i--) {
					argStr += " ";
				}
			}
			argStr += "   ";
			switch (arg.type) {
			case ArgType::STRING:
				argStr += " <string> (max " + std::to_string(arg.maxStrLen) + " chars)";
				break;
			case ArgType::INT:
				argStr += " <int> [" + std::to_string(arg.intLimits.min) + ", " + std::to_string(arg.intLimits.max) + "]";
				break;
			case ArgType::FLOAT:
				argStr += " <float> [" + std::to_string(arg.floatLimits.min) + ", " + std::to_string(arg.floatLimits.max) + "]";
				break;
			case ArgType::FLAG:
				argStr += " <flag>";
				break;
			}
			if (!arg.defaultValue.empty() && arg.optional) {
				argStr += " (default: " + arg.defaultValue + ")";
			}
			argStr += "\n                " + arg.description + "\n";
			man += argStr;
		}
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, man.c_str(), man.length());
	}

	void print(const char *str) {
		// if (!serial) return;
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, str, strlen(str));
	}

	static std::vector<Command *> cliCommands;
	static Command *activeLoopCommand;

	void setExecuteFunction(bool (*fn)(string payload, Command *cmd)) {
		executeFunction = fn;
	}
	void setLoopFunction(bool (*fn)(Command *cmd)) {
		loopFunction = fn;
	}
	void setInputFunction(bool (*fn)(string input, Command *cmd)) {
		inputFunction = fn;
	}

private:
	std::vector<CommandArg> args;
	std::vector<string> aliases;
	// KoliSerial *serial = nullptr;
	u8 serialNum = 0;

	const string name;
	const string description;

	bool (*executeFunction)(std::vector<args>, Command *cmd) = nullptr;
	bool (*loopFunction)(Command *cmd) = nullptr;
	bool (*inputFunction)(string input, Command *cmd) = nullptr;
	void (*abortFunction)(Command *cmd) = nullptr;
};
