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
