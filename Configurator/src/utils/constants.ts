/*
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

export const TRACE_COLORS_FOR_BLACK_BACKGROUND = [
	"red",
	"lime",
	"dodgerblue",
	"yellow",
	"magenta",
	"orange",
	"white",
	"cyan",
	"palegreen",
	"bisque",
	"lightpink",
]

export const DISARM_REASONS = ["Unset", "Switch", "RX Loss", "RTH Done", "Unknown"]

export const VTX_STATUS_NAMES = [
	"Offline",
	"Initializing",
	"Online",
	"Online",
	"Online",
	"Set Frequency",
	"Set Frequency",
	"Set Power",
	"Set Power",
	"Set Pitmode",
	"Set Pitmode",
]
export const VTX_BAND_NAMES = ["Boscam A", "Boscam B", "Boscam E", "Fatshark", "Raceband"]
export const VTX58_FREQ_TABLE = [
	[5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725], // Boscam A
	[5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866], // Boscam B
	[5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945], // Boscam E
	[5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880], // FatShark
	[5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917], // RaceBand
]
