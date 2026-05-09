/**
 * @file inFlightTuning.h
 * @brief TunableParameter class and related functions for in-flight tuning
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
	TunableParameter(u8 *valPtr, u32 step, u8 min, u8 max, const char *name, void (*onChange)() = nullptr);
	TunableParameter(u16 *valPtr, u32 step, u16 min, u16 max, const char *name, void (*onChange)() = nullptr);
	TunableParameter(u32 *valPtr, u32 step, u32 min, u32 max, const char *name, void (*onChange)() = nullptr);

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
