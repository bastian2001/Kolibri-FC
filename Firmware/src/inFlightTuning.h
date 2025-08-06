#pragma once
#include "typedefs.h"
#include <fixedPointInt.h>
#include <list>

enum class VariableType {
	I8,
	U8,
	I16,
	U16,
	I32,
	U32,
	FIX32,
	FIX64,
	F32,
	F64,
};

union tuningComboVar {
	u32 integer;
	fix32 fix;
	f32 floating;
};

class TunableParameter {
public:
	TunableParameter(u16 *valPtr, u32 step, u16 min, u16 max, const char *name, void (*onChange)() = nullptr);

	void increase();
	void decrease();
	void getValueString(char *str);
	char name[16];

private:
	const void *valPtr;
	void (*const onChangeFn)();
	const VariableType type;
	const tuningComboVar step;
	const tuningComboVar min;
	const tuningComboVar max;
};
extern std::list<TunableParameter> inFlightTuningParams;

void inFlightTuningInit();

void inFlightTuningLoop();
