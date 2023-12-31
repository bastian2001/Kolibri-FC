export const leBytesToInt = (bytes: number[], signed = false) => {
	let value = 0;
	for (let i = 0; i < bytes.length; i++) {
		value += bytes[i] * Math.pow(256, i);
	}
	if (signed && bytes[bytes.length - 1] & 0b10000000) {
		value -= Math.pow(256, bytes.length);
	}
	return value;
};

export type LogFrame = {
	elrs: {
		roll?: number;
		pitch?: number;
		throttle?: number;
		yaw?: number;
	};
	setpoint: {
		roll?: number;
		pitch?: number;
		throttle?: number;
		yaw?: number;
	};
	gyro: {
		roll?: number;
		pitch?: number;
		yaw?: number;
	};
	pid: {
		roll: {
			p?: number;
			i?: number;
			d?: number;
			ff?: number;
			s?: number;
		};
		pitch: {
			p?: number;
			i?: number;
			d?: number;
			ff?: number;
			s?: number;
		};
		yaw: {
			p?: number;
			i?: number;
			d?: number;
			ff?: number;
			s?: number;
		};
	};
	motors: {
		rr?: number;
		fr?: number;
		rl?: number;
		fl?: number;
	};
	altitude?: number;
	frametime?: number;
};

export type BBLog = {
	frameCount: number;
	rawFile: number[];
	flags: string[];
	frames: LogFrame[];
	version: number[];
	startTime: Date;
	ranges: {
		gyro: number;
		accel: number;
	};
	pidFrequency: number;
	frequencyDivider: number;
	rateFactors: number[][];
	pidConstants: number[][];
	pidConstantsNice: number[][];
	framesPerSecond: number;
	isExact: boolean;
};

export function getNestedProperty(
	obj: any,
	path: string,
	options: { defaultValue?: any; max?: number; min?: number } = {}
) {
	const pathParts = path.split('.');
	let current = obj;
	for (let i = 0; i < pathParts.length; i++) {
		if (current[pathParts[i]] === undefined) return options.defaultValue;
		current = current[pathParts[i]];
	}
	if (options.max !== undefined && current > options.max) return options.max;
	if (options.min !== undefined && current < options.min) return options.min;
	return current;
}

export function roundToDecimal(num: number, places: number) {
	return Math.round(num * Math.pow(10, places)) / Math.pow(10, places);
}
