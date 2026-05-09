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

export class PT1 {
	cutoffFreq: number
	sampleFreq: number
	alpha: number
	state: number
	constructor(cutoffFreq: number, sampleFreq: number) {
		this.cutoffFreq = cutoffFreq
		this.sampleFreq = sampleFreq
		if (cutoffFreq <= 0 || sampleFreq <= 0) {
			this.alpha = 1
		} else {
			const omega = (Math.PI * 2 * cutoffFreq) / sampleFreq
			this.alpha = omega / (omega + 1)
		}
		this.state = 0
	}

	update(value: number) {
		this.state = this.state + this.alpha * (value - this.state)
		return this.state
	}
}
