#include "Arduino.h"
#include "typedefs.h"
#include <variant>
#pragma once

using std::string;

void initCli();
void cliLoop();

bool parseInt(const char *str, i64 &value);
bool parseFloat(const char *str, f64 &value);

void getCliSuggestions(string input, std::vector<string> &suggestions);

enum class ArgType {
	STRING,
	SELECTION,
	INT,
	FLOAT,
	FLAG,
};

#define CLI_COLOR_WHITE "\x10"
#define CLI_COLOR_RED "\x11"
#define CLI_COLOR_GREEN "\x12"
#define CLI_COLOR_YELLOW "\x13"
#define CLI_COLOR_BLUE "\x14"
#define CLI_COLOR_MAGENTA "\x15"
#define CLI_COLOR_CYAN "\x16"
#define CLI_COLOR_GREY "\x17"

#define CLI_PROMPT "\x01" CLI_COLOR_YELLOW ">> " CLI_COLOR_WHITE

typedef struct selectionOption {
	string value;
	string description;
} SelectionOption;
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
		const std::vector<SelectionOption> *selectionOptions; // for selection arguments, the list of valid options
	};
	bool manDetailedSelection = true;
} CommandArg;
typedef struct runtimeArg {
	bool provided = false;
	std::variant<string, int, float, bool> value;
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
		if (!anonymous && name == "") return;
		if (name == "") {
			int argNum = 1;
			for (const auto &arg : args) {
				if (arg.anonymous) argNum++;
			}
			name = "$" + std::to_string(argNum);
		}
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
		if (!anonymous && name == "") return;
		if (name == "") {
			int argNum = 1;
			for (const auto &arg : args) {
				if (arg.anonymous) argNum++;
			}
			name = "$" + std::to_string(argNum);
		}
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
		if (!anonymous && name == "") return;
		if (name == "") {
			int argNum = 1;
			for (const auto &arg : args) {
				if (arg.anonymous) argNum++;
			}
			name = "$" + std::to_string(argNum);
		}
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
		if (name == "") return;
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
	void addSelectionArg(string name, char shorthand = 0, bool optional = false, bool anonymous = false, const std::vector<SelectionOption> *options = nullptr, string description = "", string defaultValue = "", bool manDetailedSelection = true) {
		if (anonymous) optional = false;
		if (!anonymous && name == "") return;
		if (name == "") {
			int argNum = 1;
			for (const auto &arg : args) {
				if (arg.anonymous) argNum++;
			}
			name = "$" + std::to_string(argNum);
		}
		CommandArg arg;
		arg.name = name;
		arg.shorthand = shorthand;
		arg.optional = optional;
		arg.anonymous = anonymous;
		arg.description = description;
		arg.defaultValue = defaultValue;
		arg.type = ArgType::SELECTION;
		arg.selectionOptions = options;
		arg.manDetailedSelection = manDetailedSelection;
		args.push_back(arg);
	}

	void execute(string payload, KoliSerial *serial);
	void abort() {
		if (abortFunction) abortFunction(this);
		print("Command aborted." CLI_PROMPT);
	};
	void loop() {
		if (loopFunction) {
			if (!loopFunction(this)) {
				activeLoopCommand = nullptr;
				if (!serial) return;
				MspMsgSetup setup{
					.serial = *serial,
					.fn = MspFn::CLI_COMMAND,
					.type = MspMsgType::RESPONSE,
					.version = serial->lastMspVersion,
				};
				sendMsp(setup, CLI_PROMPT, strlen(CLI_PROMPT));
			}
		}
	};
	void input(string input) {
		if (inputFunction) {
			if (!inputFunction(input, this)) {
				activeLoopCommand = nullptr;
				if (!serial) return;
				MspMsgSetup setup{
					.serial = *serial,
					.fn = MspFn::CLI_COMMAND,
					.type = MspMsgType::RESPONSE,
					.version = serial->lastMspVersion,
				};
				sendMsp(setup, CLI_PROMPT, strlen(CLI_PROMPT));
			}
		}
	};
	void printMan(KoliSerial *serial);

	void print(const char *str) {
		if (!serial) return;
		size_t len = strlen(str);
		size_t offset = 0;
		while (offset < len) {
			size_t chunkSize = min((size_t)480, len - offset);
			MspMsgSetup setup{
				.serial = *serial,
				.fn = MspFn::CLI_COMMAND,
				.type = MspMsgType::RESPONSE,
				.version = serial->lastMspVersion,
			};
			sendMsp(setup, str + offset, chunkSize);
			offset += chunkSize;
		}
	}

	static std::vector<Command *> cliCommands;
	static Command *activeLoopCommand;

	void setExecuteFunction(bool (*fn)(std::map<string, RuntimeArg> &args, Command *cmd)) {
		executeFunction = fn;
	}
	void setLoopFunction(bool (*fn)(Command *cmd)) {
		loopFunction = fn;
	}
	void setInputFunction(bool (*fn)(string input, Command *cmd)) {
		inputFunction = fn;
	}
	void setAbortFunction(void (*fn)(Command *cmd)) {
		abortFunction = fn;
	}

	bool nameMatches(string input) {
		if (input == name) return true;
		for (const auto &alias : aliases) {
			if (input == alias) return true;
		}
		return false;
	}

	KoliSerial *getSerial() {
		return serial;
	}

	static Command *getCommandByName(string &name) {
		// extract first token as command name

		for (Command *cmd : cliCommands) {
			if (cmd->nameMatches(name)) {
				return cmd;
			}
		}
		return nullptr;
	}

	void getSuggestions(string payload, std::vector<string> &suggestions);
	bool hasArgs() {
		return !args.empty();
	}

	const string name;
	const string description;

private:
	std::vector<CommandArg> args;
	std::vector<string> aliases;
	KoliSerial *serial = nullptr;

	bool (*executeFunction)(std::map<string, RuntimeArg> &args, Command *cmd) = nullptr;
	bool (*loopFunction)(Command *cmd) = nullptr;
	bool (*inputFunction)(string input, Command *cmd) = nullptr;
	void (*abortFunction)(Command *cmd) = nullptr;

	void fillRuntimeArgsDefault(std::map<string, RuntimeArg> &runtimeArgs);
	bool parseCommandArgs(const std::list<string> &tokens, std::map<string, RuntimeArg> &runtimeArgs, bool endsWithSpace, bool suggestMode, std::vector<string> *suggestions, string &failText);
	const CommandArg *findNextAnonymousArg(const std::map<string, RuntimeArg> &runtimeArgs);
};
