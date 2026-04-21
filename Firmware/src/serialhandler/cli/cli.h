#include "Arduino.h"
#include "typedefs.h"
#include <variant>
#pragma once

using std::string;

void initCli();
void cliLoop();

bool parseInt(const char *str, i64 &value);
bool parseFloat(const char *str, f64 &value);

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

	void execute(string payload, u8 serialNum) {
		// if (!serial) return;
		// this->serial = serial;
		this->serialNum = serialNum;

		if (activeLoopCommand) {
			print("Another command is currently running. Please wait until it's finished or abort it.");
			return;
		}

		if (executeFunction) {
			// Fill runtimeArgs with default values
			std::map<string, RuntimeArg> runtimeArgs;
			for (auto &arg : args) {
				runtimeArgs[arg.name] = (RuntimeArg){false, 0};
				auto &rArg = runtimeArgs[arg.name];
				switch (arg.type) {
				case ArgType::FLAG:
					rArg.value = false;
					break;
				case ArgType::STRING:
					rArg.value = arg.defaultValue;
					break;
				case ArgType::INT: {
					i64 defaultVal = 0;
					rArg.value = (i32)0;
					if (!parseInt(arg.defaultValue.c_str(), defaultVal)) {
						rArg.value = (i32)defaultVal;
					}
				} break;
				case ArgType::FLOAT: {
					f64 defaultVal = 0;
					rArg.value = (f32)0;
					if (!parseFloat(arg.defaultValue.c_str(), defaultVal)) {
						rArg.value = (f32)defaultVal;
					}
				} break;
				case ArgType::SELECTION:
					rArg.value = arg.defaultValue;
					break;
				}
			}

			// Tokenize payload
			std::list<string> tokens;
			string currentToken;
			bool inQuotes = false;
			bool escape = false;
			for (char c : payload) {
				if (c == '\\' && !escape) {
					escape = true;
					continue;
				}
				if (escape) {
					switch (c) {
					case 'n':
						currentToken += '\n';
						break;
					case 't':
						currentToken += '\t';
						break;
					case 'r':
						currentToken += '\r';
						break;
					case 'b':
						currentToken += '\b';
						break;
					case 'v':
						currentToken += '\v';
						break;
					default:
						currentToken += c;
						break;
					}
					escape = false;
					continue;
				}
				if (c == '\"') {
					inQuotes = !inQuotes;
				} else if (c == ' ' && !inQuotes) {
					if (!currentToken.empty()) {
						tokens.push_back(currentToken);
						currentToken.clear();
					}
				} else {
					currentToken += c;
				}
			}
			if (!currentToken.empty()) {
				tokens.push_back(currentToken);
			}

			// First, parse all non-anonymous arguments (identified by name/shorthand)
			const CommandArg *currentArg = nullptr;
			bool failed = false;
			string failText = "";
			std::list<string> anonArgTokens; // tokens that are not identified as arguments, to be parsed as anonymous arguments later
			for (auto it = tokens.begin(); it != tokens.end(); it++) {
				string token = *it;
				if (failed) break;
				if (currentArg) {
					// expect value for currentArg
					const CommandArg &arg = *currentArg;
					RuntimeArg &rArg = runtimeArgs[arg.name];
					parseArgument(token, arg, rArg, failed, failText);

					currentArg = nullptr;
					continue;
				}

				// expect argument or shorthand
				if (token.length() < 2 || token[0] != '-') {
					// not an argument, treat as anonymous argument token for now
					anonArgTokens.push_back(token);
					continue;
				}
				if (token[1] == '-') {
					// longhand
					bool found = false;
					string name = token.substr(2);
					for (const auto &arg : args) {
						if (arg.name == name) {
							found = true;
							currentArg = &arg;
							if (arg.type == ArgType::FLAG) {
								// flags don't expect a value, just set them to true
								std::get<bool>(runtimeArgs[arg.name].value) = true;
								currentArg = nullptr;
								runtimeArgs[arg.name].provided = true;
							}
							break;
						}
					}
					if (!found) {
						// unknown argument
						failed = true;
						failText = "Unknown argument: \"" + name + "\"";
						break;
					}
				} else {
					// shorthand, find the corresponding argument
					int shorthandCount = token.length() - 1;
					string finalArgName;
					for (int i = 0; i < shorthandCount; i++) {
						char shorthand = token[i + 1];
						bool found = false;
						bool isFinal = i == shorthandCount - 1;
						for (const auto &arg : args) {
							if (arg.shorthand == shorthand) {
								found = true;
								if (arg.type == ArgType::FLAG) {
									// flags don't expect a value, just set them to true
									std::get<bool>(runtimeArgs[arg.name].value) = true;
									runtimeArgs[arg.name].provided = true;
									break;
								}

								if (!isFinal) {
									// only the last shorthand in the token can expect a value, otherwise it's ambiguous (e.g. -abc could be -a -b -c or -a -bc or -ab -c)
									failed = true;
									failText = "Only the last shorthand in a token can expect a value: " + string(1, shorthand);
									break;
								} else {
									currentArg = &arg;
								}
								break;
							}
						}
						if (failed) break;
						if (!found) {
							// unknown shorthand
							failed = true;
							failText = "Unknown argument shorthand: " + string(1, shorthand);
							break;
						}
					}
				}
			}

			if (failed) return print(failText.c_str());

			// Next, parse anonymous arguments
			auto anonArgIt = anonArgTokens.begin();
			for (const auto &arg : args) {
				RuntimeArg &rArg = runtimeArgs[arg.name];
				if (arg.anonymous && rArg.provided == false) {
					if (anonArgIt == anonArgTokens.end()) {
						if (arg.optional)
							continue;
						else
							break;
					}

					parseArgument(*anonArgIt, arg, rArg, failed, failText);
					anonArgIt++;
				}
			}
			// check if any arguments are missing
			for (const auto &arg : args) {
				RuntimeArg &rArg = runtimeArgs[arg.name];
				if (!arg.optional && rArg.provided == false) {
					failed = true;
					failText = "Missing value for argument \"" + arg.name + "\"";
					break;
				}
			}
			if (failed) return print(failText.c_str());

			if (anonArgIt != anonArgTokens.end()) {
				failed = true;
				failText = "Too many arguments provided\n";
			}
			if (failed) return print(failText.c_str());

			// Finally, execute the command with the parsed arguments
			if (executeFunction(runtimeArgs, this)) {
				activeLoopCommand = this;
			}
		} else {
			const char *buf = "Command not implemented";
			sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, buf, strlen(buf));
		}
	};
	void abort() {
		if (abortFunction) abortFunction(this);
		print("Command aborted." CLI_PROMPT);
	};
	void loop() {
		if (loopFunction) {
			if (!loopFunction(this)) {
				activeLoopCommand = nullptr;
				sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, CLI_PROMPT, strlen(CLI_PROMPT));
			}
		}
	};
	void input(string input) {
		if (inputFunction) inputFunction(input, this);
	};
	void printMan(u8 serialNum) {
		// if (!serial) return;
		string man = CLI_COLOR_CYAN "NAME\n\t" CLI_COLOR_WHITE + name + " => " CLI_COLOR_GREY + description + CLI_COLOR_WHITE;
		if (!aliases.empty()) {
			man += CLI_COLOR_CYAN "\n\nALIASES\n\t" CLI_COLOR_WHITE;
			for (const auto &alias : aliases) {
				man += alias + " ";
			}
		}
		man += CLI_COLOR_CYAN "\n\nUSAGE\n\t" CLI_COLOR_WHITE + name;
		bool hasOptionalArgs = false;
		for (const auto &arg : args) {
			if (arg.optional) {
				hasOptionalArgs = true;
				continue;
			}
			if (arg.anonymous) {
				man += " <" + arg.name + ">";
			} else {
				if (arg.shorthand)
					man += " -" + string(1, arg.shorthand) + " <" + arg.name + ">";
				else
					man += " --" + arg.name + " <" + arg.name + ">";
			}
		}
		if (hasOptionalArgs) {
			man += CLI_COLOR_GREY " [...args]" CLI_COLOR_WHITE;
		}
		man += CLI_COLOR_CYAN "\n\nARGUMENTS\n" CLI_COLOR_WHITE;
		for (const auto &arg : args) {
			string argStr;
			if (!arg.anonymous) {
				if (arg.shorthand)
					argStr += "\t-" + string(1, arg.shorthand) + ", ";
				else
					argStr += "\t    ";
				argStr += "--" + arg.name;
				for (int i = 10 - arg.name.length(); i > 0; i--) {
					argStr += " ";
				}
			} else {
				argStr += "\t\t<" + arg.name + ">";
				for (int i = 10 - arg.name.length(); i > 0; i--) {
					argStr += " ";
				}
			}
			argStr += "  ";
			switch (arg.type) {
			case ArgType::STRING:
				argStr += CLI_COLOR_GREY " (string, max " + std::to_string(arg.maxStrLen) + " chars";
				break;
			case ArgType::INT:
				argStr += CLI_COLOR_GREY " (int, [" + std::to_string(arg.intLimits.min) + ", " + std::to_string(arg.intLimits.max) + "]";
				break;
			case ArgType::FLOAT:
				argStr += CLI_COLOR_GREY " (float, [" + std::to_string(arg.floatLimits.min) + ", " + std::to_string(arg.floatLimits.max);
				break;
			case ArgType::SELECTION:
				argStr += CLI_COLOR_GREY " (selection";
				break;
			case ArgType::FLAG:
				argStr += CLI_COLOR_GREY " (flag";
				break;
			}
			if (!arg.defaultValue.empty() && arg.optional) {
				argStr += ", default: " + arg.defaultValue + ")" CLI_COLOR_WHITE;
			} else {
				argStr += ")" CLI_COLOR_WHITE;
			}
			argStr += "\n\t\t\t" + arg.description + "\n";

			if (arg.type == ArgType::SELECTION && arg.selectionOptions && !arg.selectionOptions->empty()) {
				if (arg.manDetailedSelection) {
					for (size_t i = 0; i < arg.selectionOptions->size(); i++) {
						auto &opt = (*arg.selectionOptions)[i];
						argStr += "\t\t\t\t- " + opt.value;
						if (opt.description.length() > 0) {
							argStr += " => " + opt.description;
						}
						argStr += "\n";
					}
				} else {
					argStr += "\t\t\t\tOptions: ";
					for (size_t i = 0; i < arg.selectionOptions->size(); i++) {
						auto &opt = (*arg.selectionOptions)[i];
						argStr += opt.value;
						if (i != arg.selectionOptions->size() - 1) {
							argStr += ", ";
						}
					}
					argStr += "\n";
				}
			}

			man += argStr;
		}
		if (args.empty()) {
			man += CLI_COLOR_GREY "\t- None -\n" CLI_COLOR_WHITE;
		}
		print(man.c_str());
	}

	void print(const char *str) {
		// if (!serial) return;
		size_t len = strlen(str);
		size_t offset = 0;
		while (offset < len) {
			size_t chunkSize = min((size_t)480, len - offset);
			sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, str + offset, chunkSize);
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

	bool nameMatches(string input) {
		if (input == name) return true;
		for (const auto &alias : aliases) {
			if (input == alias) return true;
		}
		return false;
	}

	u8 &getSerialNum() {
		return serialNum;
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

	const string name;
	const string description;

private:
	std::vector<CommandArg> args;
	std::vector<string> aliases;
	// KoliSerial *serial = nullptr;
	u8 serialNum = 0;

	bool (*executeFunction)(std::map<string, RuntimeArg> &args, Command *cmd) = nullptr;
	bool (*loopFunction)(Command *cmd) = nullptr;
	bool (*inputFunction)(string input, Command *cmd) = nullptr;
	void (*abortFunction)(Command *cmd) = nullptr;

	static void parseArgument(string valueToken, const CommandArg &arg, RuntimeArg &rArg, bool &failed, string &failText) {
		switch (arg.type) {
		case ArgType::STRING: {
			rArg.value = valueToken;
			if (std::get<string>(rArg.value).length() > arg.maxStrLen) {
				failed = true;
				failText = "Value for argument \"" + arg.name + "\" exceeds maximum length of " + std::to_string(arg.maxStrLen) + ": " + valueToken;
			}
		} break;
		case ArgType::INT: {
			i64 value = 0;
			if (parseInt(valueToken.c_str(), value)) {
				if (value < arg.intLimits.min || value > arg.intLimits.max) {
					failed = true;
					failText = "Value for argument \"" + arg.name + "\" out of range: " + valueToken;
					break;
				}
				rArg.value = (i32)value;
			} else {
				failed = true;
				failText = "Invalid value for argument \"" + arg.name + "\": " + valueToken;
				break;
			}
		} break;
		case ArgType::FLOAT: {
			f64 value = 0;
			if (parseFloat(valueToken.c_str(), value)) {
				if (value < arg.floatLimits.min || value > arg.floatLimits.max) {
					failed = true;
					failText = "Value for argument \"" + arg.name + "\" out of range: " + valueToken;
					break;
				}
				rArg.value = (f32)value;
			} else {
				failed = true;
				failText = "Invalid value for argument \"" + arg.name + "\": " + valueToken;
				break;
			}
		} break;
		case ArgType::SELECTION: {
			if (arg.selectionOptions != nullptr && !arg.selectionOptions->empty()) {
				bool found = false;
				for (const auto &option : *arg.selectionOptions) {
					if (option.value == valueToken) {
						found = true;
						break;
					}
				}
				if (!found) {
					failed = true;
					failText = "Invalid value for argument \"" + arg.name + "\": " + valueToken;
					break;
				}
			}
			rArg.value = valueToken;
		} break;
		}
		rArg.provided = true;
	};
};
