#include "osd/osdElement.h"

// using Formatter = void (*)(u8 element, void* data, char *out);

// typedef void (*formatter)(void *data, char *out);
// typedef formatter (*dunno)();
//
// formatter getFormatter(u8 element);
//
// void testFormatter(void *data, char *out) {
// sniprintf(out, 10, "test");
// }
//
// getFormatter(u8 element){
// static dunno = testFormatter;
// return testFormatter;
// }
//
//

// typedef void (*Formatter)(void *data, char *out);

// void test1Formatter(void *data, char *out) {
// 	snprintf(out, 10, "test1");
// }

// void test2Formatter(void *data, char *out) {
// 	snprintf(out, 10, "test2");
// }

// Formatter getFormatter(u8 element) {
// 	switch (element) {
// 	case 0:
// 		return test1Formatter;
// 	case 1:
// 		return test2Formatter;
// 	default:
// 		return nullptr;
// 	}
// }

// void testFormatter() { // TODO test this concept
// 	u8 element = 0;
// 	char output[10];
// 	Serial1.println("Created formatter");
// 	Formatter formatter = getFormatter(element);
// 	Serial1.println("getFormatter(0)");
// 	formatter(nullptr, output);
// 	Serial1.println("formatter(nullptr, output)");
// 	Serial1.print("Got: ");
// 	Serial1.println(output);
// }

namespace formatter {
	using FormatFunc = void (*)(void *data, char *out);
	static void voltage(void *data, char *out) {
		snprintf("testing", 30, out);
		return;
	}

}

#include <map>

//std::map<elemType::elem, formatter::FormatFunc> formatters = {}; // TODO remove bloat
