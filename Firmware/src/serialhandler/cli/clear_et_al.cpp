#include "global.h"

void initClear(){
	Command *cmd = new Command("clear", "Clear the screen");
	cmd->addAlias("cls");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("\v"); // VT character clears the screen in our terminal emulator
		return false;
	});
	Command::cliCommands.push_back(cmd);
}

void initScreen(){
	Command *cmd = new Command("screen", "Start a new shell session (clears the screen), returning to the previous session on exit");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("\x0f"); // SI character starts a new session in our terminal emulator
		return false;
	});
	Command::cliCommands.push_back(cmd);
}

void initExit(){
	Command *cmd = new Command("exit", "Exit the current shell session, returning to the previous session if screen command was used");
	cmd->addAlias("quit");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		cmd->print("\x0e"); // SO character exits the current session in our terminal emulator
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
