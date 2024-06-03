#pragma once
#include <typedefs.h>

void runUnitTests();

template <typename T>
struct Fmt {
	static const char *f;
};

class ExpectBase {
public:
	static u32 failed;
	static u32 succeeded;
	static bool silentLogging;

	static void enableSilent(bool silent = true) {
		silentLogging = silent;
	}

	static bool printResults(bool clearResults = true, const char *category = nullptr) {
		if (!silentLogging || failed) {
			Serial.println();
			Serial.println(" =============== Test results =============== ");
			if (category)
				Serial.printf("Tests for %s\n", category);
			Serial.printf("Total tests: %d\n", failed + succeeded);
			if (failed)
				Serial.printf("Failed: %d\n", failed);
			else
				Serial.println("All tests passed!");
			Serial.println(" ============================================ ");
			Serial.println();
		}
		bool ret = failed > 0;
		if (clearResults) {
			failed = 0;
			succeeded = 0;
		}
		return ret;
	}
};

template <typename T>
class Expect : ExpectBase {
public:
	Expect(T value) : value(value) {}
	Expect withName(const char *n) { return name(n); }
	Expect name(const char *name) {
		strncpy(identifier, name, 32);
		return *this;
	}
	Expect withIndex(i32 i) { return index(i); }
	Expect index(i32 i) {
		this->ind = i;
		return *this;
	}
	Expect toEqual(T expected) {
		if (value != expected) {
			failed++;
			printFailed();
			printValueExpected("is not equal to", expected);
		} else {
			succeeded++;
			if (!silentLogging) {
				printOk();
				printValueExpected("is equal to", expected);
			}
		}
		return *this;
	}
	Expect toBeGreaterThan(T expected) {
		if (value <= expected) {
			failed++;
			printFailed();
			printValueExpected("is not greater than", expected);
		} else {
			succeeded++;
			if (!silentLogging) {
				printOk();
				printValueExpected("is greater than", expected);
			}
		}
		return *this;
	}
	Expect toBeGreaterThanOrEqual(T expected) {
		if (value < expected) {
			failed++;
			printFailed();
			printValueExpected("is not greater than or equal to", expected);
		} else {
			succeeded++;
			if (!silentLogging) {
				printOk();
				printValueExpected("is greater than or equal to", expected);
			}
		}
		return *this;
	}
	Expect toBeLessThan(T expected) {
		if (value >= expected) {
			failed++;
			printFailed();
			printValueExpected("is not less than", expected);
		} else {
			succeeded++;
			if (!silentLogging) {
				printOk();
				printValueExpected("is less than", expected);
			}
		}
		return *this;
	}
	Expect toBeLessThanOrEqual(T expected) {
		if (value > expected) {
			failed++;
			printFailed();
			printValueExpected("is not less than or equal to", expected);
		} else {
			succeeded++;
			if (!silentLogging) {
				printOk();
				printValueExpected("is less than or equal to", expected);
			}
		}
		return *this;
	}

private:
	T value;
	i32 ind = -1;
	char identifier[32] = "";

	void printOk() {
		if (ind != -1 && identifier[0] != '\0') {
			Serial.printf("Ok %s[%2d]: ", identifier, ind);
		} else if (identifier[0] != '\0') {
			Serial.printf("Ok %s: ", identifier);
		} else if (ind != -1) {
			Serial.printf("Ok [%2d]: ", ind);
		} else {
			Serial.printf("Ok: ");
		}
	}
	void printFailed() {
		if (ind != -1 && identifier[0] != '\0') {
			Serial.printf("Failed %s[%2d]: ", identifier, ind);
		} else if (identifier[0] != '\0') {
			Serial.printf("Failed %s: ", identifier);
		} else if (ind != -1) {
			Serial.printf("Failed [%2d]: ", ind);
		} else {
			Serial.printf("Failed: ");
		}
	}
	void printValueExpected(const char *msg, T expected) {
		Serial.printf(Fmt<T>::f, value);
		Serial.print(' ');
		Serial.print(msg);
		Serial.print(' ');
		Serial.printf(Fmt<T>::f, expected);
		Serial.println();
	}
};