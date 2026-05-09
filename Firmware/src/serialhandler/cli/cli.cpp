/**
 * @file cli.cpp
 * @brief General CLI functions, and partial implementation of Command class
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"

std::vector<Command *> Command::cliCommands;
Command *Command::activeLoopCommand;

void initClear();
void initEcho();
void initExit();
void initGet();
void initGyroCalibration();
void initHelp();
void initMan();
void initPrint();
void initReboot();
void initReset();
void initSave();
void initScreen();
void initSerialStats();
void initSet();
void initStatus();

void finishInitMan();

void initCli() {
	initClear();
	initEcho();
	initExit();
	initGet();
	initGyroCalibration();
	initHelp();
	initMan();
	initPrint();
	initReboot();
	initReset();
	initSave();
	initScreen();
	initSerialStats();
	initSet();
	initStatus();

	finishInitMan();
}

void cliLoop() {
	if (Command::activeLoopCommand) {
		Command::activeLoopCommand->loop();
	}
}

void getCliSuggestions(string input, std::vector<string> &suggestions) {
	size_t spaceIndex = input.find(' ');
	string cName = spaceIndex == string::npos ? input : input.substr(0, spaceIndex);
	string args = spaceIndex == string::npos ? "" : input.substr(spaceIndex + 1);
	if (spaceIndex == string::npos) {
		for (Command *cmd : Command::cliCommands) {
			if (cmd->nameMatches(input)) {
				suggestions.push_back(cmd->name + (cmd->hasArgs() ? " " : ""));
			} else if (cmd->name.find(input) == 0) {
				suggestions.push_back(cmd->name + (cmd->hasArgs() ? " " : ""));
			}
		}
		for (Command *cmd : Command::cliCommands) {
			const string &name = cmd->name;
			bool alreadyIn = false;
			for (const auto &s : suggestions) {
				if (s == name || s == name + " ") {
					alreadyIn = true;
					break;
				}
			}
			if (alreadyIn) continue;

			size_t cmdPos = 0;
			for (size_t inPos = 0; inPos < input.length(); inPos++) {
				size_t foundPos = name.find(input[inPos], cmdPos);
				if (foundPos == string::npos) {
					break;
				}
				cmdPos = foundPos + 1;
				if (inPos == input.length() - 1) {
					suggestions.push_back(cmd->name + (cmd->hasArgs() ? " " : ""));
					break;
				}
			}
		}
	} else {
		Command *cmd = nullptr;
		for (Command *c : Command::cliCommands) {
			if (c->nameMatches(cName)) {
				cmd = c;
				break;
			}
		}
		if (!cmd) return;

		cmd->getSuggestions(args, suggestions);
	}
}

// =================== Command class implementation ===================
static void parseArgument(string valueToken, const CommandArg &arg, RuntimeArg &rArg, bool &failed, string &failText);
enum class TokenType {
	None,
	ArgName,
	Value,
};

static void tokenize(string input, std::list<string> &tokens) {
	string currentToken;
	bool inQuotes = false;
	bool escape = false;
	for (char c : input) {
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
}

static bool startsWith(const string &value, const string &prefix) {
	if (prefix.empty()) return true;
	if (value.length() < prefix.length()) return false;
	return value.compare(0, prefix.length(), prefix) == 0;
}

static string formatArgHint(const CommandArg &arg) {
	switch (arg.type) {
	case ArgType::INT:
		return "[" + std::to_string(arg.intLimits.min) + ", " + std::to_string(arg.intLimits.max) + "]";
	case ArgType::FLOAT:
		return "[" + std::to_string(arg.floatLimits.min) + ", " + std::to_string(arg.floatLimits.max) + "]";
	case ArgType::STRING:
		return "[max. " + std::to_string(arg.maxStrLen) + " chars]";
	default:
		return "";
	}
}

static string formatValueHint(const CommandArg &arg) {
	string hint = formatArgHint(arg);
	if (hint.empty()) return "";
	return "<" + arg.name + "> " + hint;
}

static string escapeArgValue(const string &value);

const CommandArg *Command::findNextAnonymousArg(const std::map<string, RuntimeArg> &runtimeArgs) {
	for (const auto &arg : args) {
		if (!arg.anonymous) continue;
		auto it = runtimeArgs.find(arg.name);
		if (it == runtimeArgs.end()) continue;
		if (!it->second.provided) return &arg;
	}
	return nullptr;
}

static string escapeArgValue(const string &value) {
	string escaped;
	bool hasSpace = false;
	for (char c : value) {
		switch (c) {
		case '\n':
			escaped += "\\n";
			break;
		case '\t':
			escaped += "\\t";
			break;
		case '\r':
			escaped += "\\r";
			break;
		case '\b':
			escaped += "\\b";
			break;
		case '\v':
			escaped += "\\v";
			break;
		case ' ':
			escaped += ' ';
			hasSpace = true;
			break;
		default:
			if (c == '\\' || c == '\"') {
				escaped += '\\';
			}
			escaped += c;
			break;
		}
	}
	if (hasSpace) {
		escaped = "\"" + escaped + "\"";
	}
	return escaped;
}

bool Command::parseCommandArgs(const std::list<string> &tokens, std::map<string, RuntimeArg> &runtimeArgs, bool endsWithSpace, bool suggestMode, std::vector<string> *suggestions, string &failText) {
	bool failed = false;
	const CommandArg *currentArg = nullptr;
	std::list<string> anonArgTokens;
	TokenType tokenType = TokenType::None;
	const string *partialToken = nullptr;
	bool partialIsAnonymous = false;
	failText.clear();

	for (auto it = tokens.begin(); it != tokens.end(); it++) {
		const string &token = *it;
		bool isLast = std::next(it) == tokens.end();
		if (failed) break;

		if (currentArg) {
			if (suggestMode && isLast && !endsWithSpace) {
				tokenType = TokenType::Value;
				partialIsAnonymous = false;
				partialToken = &token;
				break;
			}
			RuntimeArg &rArg = runtimeArgs[currentArg->name];
			parseArgument(token, *currentArg, rArg, failed, failText);
			if (failed) break;
			currentArg = nullptr;
			continue;
		}

		if (token.length() < 2 || token[0] != '-') {
			if (suggestMode && isLast && !endsWithSpace) {
				tokenType = TokenType::Value;
				partialIsAnonymous = true;
				partialToken = &token;
				break;
			}
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
				if (suggestMode && isLast && !endsWithSpace) {
					tokenType = TokenType::ArgName;
					partialToken = &token;
					break;
				}
				failed = true;
				failText = "Unknown argument: \"" + name + "\"";
				break;
			}
		} else {
			// shorthand, find the corresponding argument
			int shorthandCount = token.length() - 1;
			bool unknownShorthand = false;
			char unknownShorthandChar = 0;
			for (int i = 0; i < shorthandCount; i++) {
				char shorthand = token[i + 1];
				bool found = false;
				bool isFinal = i == shorthandCount - 1;
				for (const auto &arg : args) {
					if (arg.anonymous || arg.shorthand != shorthand) continue;
					found = true;
					if (arg.type == ArgType::FLAG) {
						std::get<bool>(runtimeArgs[arg.name].value) = true;
						runtimeArgs[arg.name].provided = true;
						break;
					}

					if (!isFinal) {
						failed = true;
						failText = "Only the last shorthand in a token can expect a value: " + string(1, shorthand);
						break;
					} else {
						currentArg = &arg;
					}
					break;
				}
				if (failed) break;
				if (!found) {
					unknownShorthand = true;
					unknownShorthandChar = shorthand;
					break;
				}
			}
			if (failed) break;
			if (unknownShorthand) {
				if (suggestMode && isLast && !endsWithSpace) {
					tokenType = TokenType::ArgName;
					partialToken = &token;
					break;
				}
				failed = true;
				failText = "Unknown argument shorthand: " + string(1, unknownShorthandChar);
				break;
			}
		}
	}

	if (failed) return false;

	// Parse anonymous arguments
	auto anonArgIt = anonArgTokens.begin();
	for (const auto &arg : args) {
		RuntimeArg &rArg = runtimeArgs[arg.name];
		if (arg.anonymous && rArg.provided == false) {
			if (anonArgIt == anonArgTokens.end()) break;
			parseArgument(*anonArgIt, arg, rArg, failed, failText);
			if (failed) break;
			anonArgIt++;
		}
	}
	if (failed) return false;

	if (!suggestMode) {
		for (const auto &arg : args) {
			RuntimeArg &rArg = runtimeArgs[arg.name];
			if (!arg.optional && rArg.provided == false) {
				failed = true;
				failText = "Missing value for argument \"" + arg.name + "\"";
				break;
			}
		}
		if (failed) return false;

		if (anonArgIt != anonArgTokens.end()) {
			failed = true;
			failText = "Too many arguments provided\n";
		}
		if (failed) return false;
	}

	if (suggestMode && suggestions) {
		const CommandArg *valueArg = nullptr;
		string valuePrefix;

		bool omitLastToken = partialToken != nullptr;
		string fullPrefix = this->name + " ";
		auto it = tokens.begin();
		size_t includeCount = tokens.size() - ((omitLastToken && !tokens.empty()) ? 1 : 0);
		for (size_t i = 0; i < includeCount; i++, it++) {
			fullPrefix += escapeArgValue(*it) + " ";
		}

		if (tokenType == TokenType::Value && partialToken) {
			valuePrefix = *partialToken;
			if (partialIsAnonymous) {
				valueArg = findNextAnonymousArg(runtimeArgs);
			} else {
				valueArg = currentArg;
			}
		} else if (currentArg) {
			valueArg = currentArg;
		} else if (tokenType == TokenType::None) {
			valueArg = findNextAnonymousArg(runtimeArgs);
		}

		if (tokenType == TokenType::ArgName && partialToken) {
			if (partialToken->length() >= 2 && (*partialToken)[0] == '-' && (*partialToken)[1] == '-') {
				string argPrefix = partialToken->substr(2);
				for (int pass = 0; pass < 2; pass++) {
					bool wantOptional = pass == 1;
					for (const auto &arg : args) {
						if (arg.anonymous) continue;
						if (arg.optional != wantOptional) continue;
						if (runtimeArgs[arg.name].provided) continue;
						if (!startsWith(arg.name, argPrefix)) continue;

						string suggestionToken = "--" + arg.name + " ";
						string meta;
						if (arg.type == ArgType::INT || arg.type == ArgType::FLOAT || arg.type == ArgType::STRING) {
							meta = formatArgHint(arg);
						}
						string suggestion = fullPrefix + suggestionToken;
						if (!meta.empty()) suggestion += "\r" + meta;
						suggestions->push_back(suggestion);
					}
				}
			}
		} else if (valueArg) {
			switch (valueArg->type) {
			case ArgType::SELECTION:
				if (valueArg->selectionOptions != nullptr && !valueArg->selectionOptions->empty()) {
					for (const auto &option : *valueArg->selectionOptions) {
						if (!startsWith(option.value, valuePrefix)) continue;
						string suggestionToken = escapeArgValue(option.value);
						string suggestion = fullPrefix + suggestionToken + " ";
						suggestions->push_back(suggestion);
					}
				}
				break;
			case ArgType::INT:
			case ArgType::FLOAT:
			case ArgType::STRING: {
				string meta = formatValueHint(*valueArg);
				if (!meta.empty()) {
					string insertPrefix = fullPrefix;
					if (tokenType == TokenType::Value && partialToken && !partialToken->empty()) {
						insertPrefix = fullPrefix + escapeArgValue(*partialToken) + " ";
					}
					suggestions->push_back(insertPrefix + "\r" + meta);
				}
			} break;
			case ArgType::FLAG:
				break;
			}
		} else {
			for (int pass = 0; pass < 2; pass++) {
				bool wantOptional = pass == 1;
				for (const auto &arg : args) {
					if (arg.anonymous) continue;
					if (arg.optional != wantOptional) continue;
					if (runtimeArgs[arg.name].provided) continue;
					string suggestionToken = "--" + arg.name;
					string meta;
					if (arg.type != ArgType::FLAG) {
						suggestionToken += " ";
					}
					if (arg.type == ArgType::INT || arg.type == ArgType::FLOAT || arg.type == ArgType::STRING) {
						meta = formatArgHint(arg);
					}
					string suggestion = fullPrefix + suggestionToken + " ";
					if (!meta.empty()) suggestion += "\r" + meta;
					suggestions->push_back(suggestion);
				}
			}
		}
	}

	return true;
}

void Command::execute(string payload, u8 serialNum) {
	// if (!serial) return;
	// this->serial = serial;
	this->serialNum = serialNum;

	if (activeLoopCommand) {
		print("Another command is currently running. Please wait until it's finished or abort it.");
		return;
	}

	if (executeFunction) {
		std::map<string, RuntimeArg> runtimeArgs;
		this->fillRuntimeArgsDefault(runtimeArgs);

		std::list<string> tokens;
		tokenize(payload, tokens);
		string failText;
		if (!parseCommandArgs(tokens, runtimeArgs, false, false, nullptr, failText)) {
			return print((CLI_COLOR_RED + failText).c_str());
		}

		// Finally, execute the command with the parsed arguments
		if (executeFunction(runtimeArgs, this)) {
			activeLoopCommand = this;
		}
	} else {
		const char *buf = "Command not implemented";
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, buf, strlen(buf));
	}
};

void Command::printMan(u8 serialNum) {
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

void Command::getSuggestions(string payload, std::vector<string> &suggestions) {
	std::map<string, RuntimeArg> runtimeArgs;
	this->fillRuntimeArgsDefault(runtimeArgs);

	std::list<string> tokens;
	tokenize(payload, tokens);
	bool endsWithSpace = !payload.empty() && payload[payload.size() - 1] == ' ';
	string failText;
	parseCommandArgs(tokens, runtimeArgs, endsWithSpace, true, &suggestions, failText);
}

static void parseArgument(string valueToken, const CommandArg &arg, RuntimeArg &rArg, bool &failed, string &failText) {
	switch (arg.type) {
	case ArgType::STRING: {
		rArg.value = valueToken;
		if (valueToken.length() > arg.maxStrLen) {
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
}

void Command::fillRuntimeArgsDefault(std::map<string, RuntimeArg> &runtimeArgs) {
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
}
