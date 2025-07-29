// #include "osd/osdElement.h"

// // using Formatter = void (*)(u8 element, void* data, char *out);

// // typedef void (*formatter)(void *data, char *out);
// // typedef formatter (*dunno)();
// //
// // formatter getFormatter(u8 element);
// //
// // void testFormatter(void *data, char *out) {
// // sniprintf(out, 10, "test");
// // }
// //
// // getFormatter(u8 element){
// // static dunno = testFormatter;
// // return testFormatter;
// // }
// //
// //

// // typedef void (*Formatter)(void *data, char *out);

// // void test1Formatter(void *data, char *out) {
// // 	snprintf(out, 10, "test1");
// // }

// // void test2Formatter(void *data, char *out) {
// // 	snprintf(out, 10, "test2");
// // }

// // Formatter getFormatter(u8 element) {
// // 	switch (element) {
// // 	case 0:
// // 		return test1Formatter;
// // 	case 1:
// // 		return test2Formatter;
// // 	default:
// // 		return nullptr;
// // 	}
// // }

// // void testFormatter() { // TODO test this concept
// // 	u8 element = 0;
// // 	char output[10];
// // 	Serial1.println("Created formatter");
// // 	Formatter formatter = getFormatter(element);
// // 	Serial1.println("getFormatter(0)");
// // 	formatter(nullptr, output);
// // 	Serial1.println("formatter(nullptr, output)");
// // 	Serial1.print("Got: ");
// // 	Serial1.println(output);
// // }

// namespace formatter {
// #define MAX_LENGTH 30
// 	enum class UnitOfMeasurement : u8 {
// 		NONE,
// 		// Distance
// 		METERS,
// 		KILOMETERS,
// 		// Speed
// 		METERS_PER_SECOND,
// 		KILOMETERS_PER_HOUR,
// 		// Electricity
// 		MILLIVOLTS,
// 		AMPS,
// 		MILLIAMPHOURS,
// 		// Temperature
// 		CELSIUS,
// 		FAHRENHEIT,
// 		KELVIN,
// 		// Pressure
// 		PASCAL,
// 		BAR,
// 		HECTOPASCAL,

// 		PERCENTAGE,
// 		DBM
// 	};

// 	using FormatFunc = void (*)(void *data, char *out, UnitOfMeasurement unit);

// 	static void undefined(void *data, char *out, UnitOfMeasurement unit) {
// 		snprintf(out, MAX_LENGTH, "No formatter found");
// 		return;
// 	}

// 	static void valueWithUnit(void *data, char *out, UnitOfMeasurement unit = UnitOfMeasurement::NONE) {
// 		snprintf(out, MAX_LENGTH, "err:implement");
// 		return;
// 	}

// 	// static void voltage_mV(void *data, char *out) {
// 	// 	snprintf(out, MAX_LENGTH, "err:implement");
// 	// 	return;
// 	// }

// 	// static void current_A(void *data, char *out) {
// 	// 	snprintf(out, MAX_LENGTH, "err:implement");
// 	// 	return;
// 	// }

// 	// static void speed_m_s(void *data, char *out) {
// 	// 	snprintf(out, MAX_LENGTH, "err:implement");
// 	// 	return;
// 	// }
// 	// static void altitude_m(void *data, char *out) {
// 	// 	snprintf(out, MAX_LENGTH, "err:implement");
// 	// 	return;
// 	// }

// 	// static void gps_location(void *data, char *out) { //?Might also be plain text
// 	// 	snprintf(out, MAX_LENGTH, "err:implement");
// 	// 	return;
// 	// }

// 	// static void rssi_dbm(void *data, char *out) {
// 	// 	snprintf(out, MAX_LENGTH, "err:implement");
// 	// 	return;
// 	// }

// 	static void plainText(void *data, char *out) {
// 		snprintf(out, MAX_LENGTH, "err:implement");
// 		return;
// 	}

// 	FormatFunc getFormatter(ElementType element) {
// 		auto idx = formatters.find(element);
// 		if (idx != formatters.end()) {
// 			return idx->second;
// 		}
// 		return nullptr;
// 	}

// 	std::map<ElementType, UnitOfMeasurement> unitsOfMeasurement = {
// 		{ElementType::BATTERY_VOLTAGE, UnitOfMeasurement::MILLIVOLTS},
// 		// Add other units here
// 	};

// 	std::map<ElementType, formatter::FormatFunc> formatters = {
// 		{ElementType::BATTERY_VOLTAGE, formatter::valueWithUnit},
// 		// Add other formatters here
// 	}; // TODO remove bloat

// }
// #include <map> //TODO move
