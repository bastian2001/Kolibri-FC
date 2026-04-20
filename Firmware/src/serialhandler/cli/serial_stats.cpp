#include "global.h"

void initSerialStats() {
	Command *cmd = new Command("serial_stats", "Get or reset statistics about the serial ports");
	cmd->addFlagArg("reset", 'r', "Reset the serial statistics after printing");
	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		const auto &resetArg = args["reset"];
		bool reset = std::get<bool>(resetArg.value);
		string response = "";

		f32 timeSinceReset = f32(KoliSerial::sinceReset) / 1000.f;
		response = "Statistics since the last reset (" + std::to_string((u32)timeSinceReset) + "ms ago)\n";
		for (int i = 0; i < SERIAL_COUNT; i++) {
			char line[128];
			if (!serials[i]) continue;
			KoliSerial &s = *serials[i];
			snprintf(line, 128, "Serial %d (%s):     TX: %7d bytes (%.2fKB/s),     RX: %7d bytes (%.2fKB/s),     ", i, KoliSerial::serialTypeNames[(u8)s.serialType], s.totalTx, s.totalTx / timeSinceReset, s.totalRx, s.totalRx / timeSinceReset);
			response += line;
			bool firstFunction = true;
			for (int j = 0; j < SERIAL_FUNCTION_COUNT; j++) {
				if (s.functions & (1 << j)) {
					if (firstFunction)
						firstFunction = false;
					else
						response += ", ";
					response += serialFunctionNames[j];
				}
			}
			response += '\n';
		}

		if (reset) {
			for (auto &s : serials) {
				if (!s) continue;
				while (s->totalRx || s->totalTx) {
					// clearing may fail in case a byte is read/written atm
					s->totalRx = 0;
					s->totalTx = 0;
				}
			}
			KoliSerial::sinceReset = 0;
			response += "\nSerial statistics reset";
		}

		cmd->print(response.c_str());
		return false;
	});
	Command::cliCommands.push_back(cmd);
}
