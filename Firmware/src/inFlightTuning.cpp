/**
 * @file inFlightTuning.cpp
 * @brief Implementation of in-flight tuning (TunableParameter class and related functions)
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

std::list<TunableParameter> inFlightTuningParams;

static bool adjusting = false;
static std::list<TunableParameter>::iterator currentParameter;

static i8 lastSelectState = 0;
static i8 lastAdjustState = 0;

TunableParameter::TunableParameter(u8 *valPtr, u32 step, u8 min, u8 max, const char *name, void (*onChange)())
	: onChangeFn(onChange),
	  valPtr(valPtr),
	  step({.integer = step}),
	  type(VariableType::U8),
	  min({.integer = min}),
	  max({.integer = max}) {
	strncpy(this->name, name, 16);
}

TunableParameter::TunableParameter(u16 *valPtr, u32 step, u16 min, u16 max, const char *name, void (*onChange)())
	: onChangeFn(onChange),
	  valPtr(valPtr),
	  step({.integer = step}),
	  type(VariableType::U16),
	  min({.integer = min}),
	  max({.integer = max}) {
	strncpy(this->name, name, 16);
}

TunableParameter::TunableParameter(u32 *valPtr, u32 step, u32 min, u32 max, const char *name, void (*onChange)())
	: onChangeFn(onChange),
	  valPtr(valPtr),
	  step({.integer = step}),
	  type(VariableType::U32),
	  min({.integer = min}),
	  max({.integer = max}) {
	strncpy(this->name, name, 16);
}

void TunableParameter::increase() {
	switch (this->type) {
	case VariableType::U8: {
		i32 val = *(u8 *)valPtr;
		val += step.integer;
		if (val > max.integer || val < min.integer) {
			val = max.integer;
		}
		*(u8 *)valPtr = val;
	} break;
	case VariableType::U16: {
		i32 val = *(u16 *)valPtr;
		val += step.integer;
		if (val > max.integer || val < min.integer) {
			val = max.integer;
		}
		*(u16 *)valPtr = val;
	} break;
	case VariableType::U32: {
		i32 val = *(u32 *)valPtr;
		val += step.integer;
		if (val > max.integer || val < min.integer) {
			val = max.integer;
		}
		*(u32 *)valPtr = val;
	} break;
	}

	if (onChangeFn != nullptr) onChangeFn();
}
void TunableParameter::decrease() {
	switch (this->type) {
	case VariableType::U8: {
		i32 val = *(u8 *)valPtr;
		val -= step.integer;
		if (val > max.integer || val < min.integer) {
			val = max.integer;
		}
		*(u8 *)valPtr = val;
	} break;
	case VariableType::U16: {
		i32 val = *(u16 *)valPtr;
		val -= step.integer;
		if (val > max.integer || val < min.integer) {
			val = max.integer;
		}
		*(u16 *)valPtr = val;
	} break;
	case VariableType::U32: {
		i32 val = *(u32 *)valPtr;
		val -= step.integer;
		if (val > max.integer || val < min.integer) {
			val = max.integer;
		}
		*(u32 *)valPtr = val;
	} break;
	}

	if (onChangeFn != nullptr) onChangeFn();
}
void TunableParameter::getValueString(char *str) {
	switch (this->type) {
	case VariableType::U8: {
		i32 val = *(u8 *)valPtr;
		snprintf(str, 16, "%d", val);
	} break;
	case VariableType::U16: {
		i32 val = *(u16 *)valPtr;
		snprintf(str, 16, "%d", val);
	} break;
	case VariableType::U32: {
		i32 val = *(u32 *)valPtr;
		snprintf(str, 16, "%d", val);
	} break;
	}
}

static void updateInFlightTuningOsd() {
	if (!adjusting) return updateElem(OSDElem::IN_FLIGHT_TUNING, "               ");
	char buf[16];
	char valueString[16];
	currentParameter->getValueString(valueString);
	int i = snprintf(buf, 16, "%s %s", currentParameter->name, valueString);
	for (; i < 15; i++) {
		buf[i] = ' ';
	}
	buf[15] = '\0';
	updateElem(OSDElem::IN_FLIGHT_TUNING, buf);
}

void inFlightTuningInit() {
	placeElem(OSDElem::IN_FLIGHT_TUNING, 15, 1);
	enableElem(OSDElem::IN_FLIGHT_TUNING);
}

void inFlightTuningLoop() {
	if (!elrs || !elrs->isLinkUp) return;
	if (!(elrs->newPacketFlag & (1 << 2))) return;
	elrs->newPacketFlag &= ~(1 << 2);
	i8 newSelectState = 0;
	if (rxModes[RxModeIndex::TUNING_NEXT_VAR].isActive()) newSelectState = 1;
	if (rxModes[RxModeIndex::TUNING_PREV_VAR].isActive()) newSelectState = -1;

	if (newSelectState != lastSelectState) {
		if (adjusting) {
			if (newSelectState == 1) {
				currentParameter++;
				if (currentParameter == inFlightTuningParams.end()) {
					adjusting = false;
				}
				updateInFlightTuningOsd();
			} else if (newSelectState == -1) {
				if (currentParameter == inFlightTuningParams.begin()) {
					adjusting = false;
				} else {
					currentParameter--;
				}
				updateInFlightTuningOsd();
			}
		} else {
			if (newSelectState == 1) {
				adjusting = true;
				currentParameter = inFlightTuningParams.begin();
				updateInFlightTuningOsd();
			} else if (newSelectState == -1) {
				adjusting = true;
				currentParameter = inFlightTuningParams.end();
				currentParameter--;
				updateInFlightTuningOsd();
			}
		}
		lastSelectState = newSelectState;
	}

	i8 newAdjustState = 0;
	if (rxModes[RxModeIndex::TUNING_INC_VAL].isActive()) newAdjustState = 1;
	if (rxModes[RxModeIndex::TUNING_DEC_VAL].isActive()) newAdjustState = -1;

	if (newAdjustState != lastAdjustState) {
		if (newAdjustState == 1 && adjusting) {
			currentParameter->increase();
			updateInFlightTuningOsd();
		} else if (newAdjustState == -1 && adjusting) {
			currentParameter->decrease();
			updateInFlightTuningOsd();
		}
		lastAdjustState = newAdjustState;
	}
}
