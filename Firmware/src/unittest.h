#include <typedefs.h>

void runUnitTests();

class Expect {
public:
	Expect(i32 value) : value(value) {}
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
	Expect toEqual(i32 expected) {
		if (value != expected) {
			failed++;
			if (ind != -1 && identifier[0] != '\0') {
				Serial.printf("Failed %s[%2d]: %4d is not equal to %d\n", identifier, ind, value, expected);
			} else if (identifier[0] != '\0') {
				Serial.printf("Failed %s: %4d is not equal to %d\n", identifier, value, expected);
			} else if (ind != -1) {
				Serial.printf("Failed [%2d]: %4d is not equal to %d\n", ind, value, expected);
			} else {
				Serial.printf("Failed: %4d is not equal to %d\n", value, expected);
			}
		} else {
			succeeded++;
			if (!silentLogging) {
				if (ind != -1 && identifier[0] != '\0') {
					Serial.printf("Ok %s[%2d]: %4d is equal to %d\n", identifier, ind, value, expected);
				} else if (identifier[0] != '\0') {
					Serial.printf("Ok %s: %4d is equal to %d\n", identifier, value, expected);
				} else if (ind != -1) {
					Serial.printf("Ok [%2d]: %4d is equal to %d\n", ind, value, expected);
				} else {
					Serial.printf("Ok: %4d is equal to %d\n", value, expected);
				}
			}
		}
		return *this;
	}
	Expect toBeGreaterThan(i32 expected) {
		if (value <= expected) {
			failed++;
			if (ind != -1 && identifier[0] != '\0') {
				Serial.printf("Failed %s[%2d]: %4d is not greater than %d\n", identifier, ind, value, expected);
			} else if (identifier[0] != '\0') {
				Serial.printf("Failed %s: %4d is not greater than %d\n", identifier, value, expected);
			} else if (ind != -1) {
				Serial.printf("Failed [%2d]: %4d is not greater than %d\n", ind, value, expected);
			} else {
				Serial.printf("Failed: %4d is not greater than %d\n", value, expected);
			}
		} else {
			succeeded++;
			if (!silentLogging) {
				if (ind != -1 && identifier[0] != '\0') {
					Serial.printf("Ok %s[%2d]: %4d is greater than %d\n", identifier, ind, value, expected);
				} else if (identifier[0] != '\0') {
					Serial.printf("Ok %s: %4d is greater than %d\n", identifier, value, expected);
				} else if (ind != -1) {
					Serial.printf("Ok [%2d]: %4d is greater than %d\n", ind, value, expected);
				} else {
					Serial.printf("Ok: %4d is greater than %d\n", value, expected);
				}
			}
		}
		return *this;
	}
	Expect toBeGreaterThanOrEqual(i32 expected) {
		if (value < expected) {
			failed++;
			if (ind != -1 && identifier[0] != '\0') {
				Serial.printf("Failed %s[%2d]: %4d is not greater than or equal to %d\n", identifier, ind, value, expected);
			} else if (identifier[0] != '\0') {
				Serial.printf("Failed %s: %4d is not greater than or equal to %d\n", identifier, value, expected);
			} else if (ind != -1) {
				Serial.printf("Failed [%2d]: %4d is not greater than or equal to %d\n", ind, value, expected);
			} else {
				Serial.printf("Failed: %4d is not greater than or equal to %d\n", value, expected);
			}
		} else {
			succeeded++;
			if (!silentLogging) {
				if (ind != -1 && identifier[0] != '\0') {
					Serial.printf("Ok %s[%2d]: %4d is greater than or equal to %d\n", identifier, ind, value, expected);
				} else if (identifier[0] != '\0') {
					Serial.printf("Ok %s: %4d is greater than or equal to %d\n", identifier, value, expected);
				} else if (ind != -1) {
					Serial.printf("Ok [%2d]: %4d is greater than or equal to %d\n", ind, value, expected);
				} else {
					Serial.printf("Ok: %4d is greater than or equal to %d\n", value, expected);
				}
			}
		}
		return *this;
	}
	Expect toBeLessThan(i32 expected) {
		if (value >= expected) {
			failed++;
			if (ind != -1 && identifier[0] != '\0') {
				Serial.printf("Failed %s[%2d]: %4d is not less than %d\n", identifier, ind, value, expected);
			} else if (identifier[0] != '\0') {
				Serial.printf("Failed %s: %4d is not less than %d\n", identifier, value, expected);
			} else if (ind != -1) {
				Serial.printf("Failed [%2d]: %4d is not less than %d\n", ind, value, expected);
			} else {
				Serial.printf("Failed: %4d is not less than %d\n", value, expected);
			}
		} else {
			succeeded++;
			if (!silentLogging) {
				if (ind != -1 && identifier[0] != '\0') {
					Serial.printf("Ok %s[%2d]: %4d is less than %d\n", identifier, ind, value, expected);
				} else if (identifier[0] != '\0') {
					Serial.printf("Ok %s: %4d is less than %d\n", identifier, value, expected);
				} else if (ind != -1) {
					Serial.printf("Ok [%2d]: %4d is less than %d\n", ind, value, expected);
				} else {
					Serial.printf("Ok: %4d is less than %d\n", value, expected);
				}
			}
		}
		return *this;
	}
	Expect toBeLessThanOrEqual(i32 expected) {
		if (value > expected) {
			failed++;
			if (ind != -1 && identifier[0] != '\0') {
				Serial.printf("Failed %s[%2d]: %4d is not less than or equal to %d\n", identifier, ind, value, expected);
			} else if (identifier[0] != '\0') {
				Serial.printf("Failed %s: %4d is not less than or equal to %d\n", identifier, value, expected);
			} else if (ind != -1) {
				Serial.printf("Failed [%2d]: %4d is not less than or equal to %d\n", ind, value, expected);
			} else {
				Serial.printf("Failed: %4d is not less than or equal to %d\n", value, expected);
			}
		} else {
			succeeded++;
			if (!silentLogging) {
				if (ind != -1 && identifier[0] != '\0') {
					Serial.printf("Ok %s[%2d]: %4d is less than or equal to %d\n", identifier, ind, value, expected);
				} else if (identifier[0] != '\0') {
					Serial.printf("Ok %s: %4d is less than or equal to %d\n", identifier, value, expected);
				} else if (ind != -1) {
					Serial.printf("Ok [%2d]: %4d is less than or equal to %d\n", ind, value, expected);
				} else {
					Serial.printf("Ok: %4d is less than or equal to %d\n", value, expected);
				}
			}
		}
		return *this;
	}

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
			failed    = 0;
			succeeded = 0;
		}
		return ret;
	}

private:
	i32 value;
	static u32 failed;
	static u32 succeeded;
	i32 ind             = -1;
	char identifier[32] = "";
	static bool silentLogging;
};