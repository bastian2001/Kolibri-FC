export type LogFrame = {
	elrs: {
		roll?: number
		pitch?: number
		throttle?: number
		yaw?: number
	}
	setpoint: {
		roll?: number
		pitch?: number
		throttle?: number
		yaw?: number
		vvel?: number
	}
	gyro: {
		roll?: number
		pitch?: number
		yaw?: number
	}
	pid: {
		roll: {
			p?: number
			i?: number
			d?: number
			ff?: number
			s?: number
		}
		pitch: {
			p?: number
			i?: number
			d?: number
			ff?: number
			s?: number
		}
		yaw: {
			p?: number
			i?: number
			d?: number
			ff?: number
			s?: number
		}
	}
	motors: {
		out: {
			rr?: number
			fr?: number
			rl?: number
			fl?: number
		}
		rpm: {
			rr?: number
			fr?: number
			rl?: number
			fl?: number
		}
	}
	frametime?: number
	attitude: {
		roll?: number
		pitch?: number
		yaw?: number
	}
	motion: {
		altitude?: number
		baro: {
			raw?: number
			hpa?: number
			alt?: number
		}
		vvel?: number
		hvel: {
			n?: number
			e?: number
		}
		magHeading?: number
		combinedHeading?: number
		accelRaw: {
			x?: number
			y?: number
			z?: number
		}
		accelFiltered: {
			x?: number
			y?: number
			z?: number
		}
		accelVertical?: number
		gps: {
			year?: number
			month?: number
			day?: number
			hour?: number
			minute?: number
			second?: number
			time_validity_flags?: number
			t_acc?: number
			ns?: number
			fix_type?: number
			flags?: number
			flags2?: number
			sat_count?: number
			lon?: number
			lat?: number
			alt?: number
			h_acc?: number
			v_acc?: number
			vel_n?: number
			vel_e?: number
			vel_d?: number
			g_speed?: number
			head_mot?: number
			s_acc?: number
			head_acc?: number
			p_dop?: number
			flags3?: number
		}
	}
	flightMode?: number
}

export type BBLog = {
	frameCount: number
	rawFile: number[]
	flags: string[]
	frames: LogFrame[]
	version: number[]
	startTime: Date
	ranges: {
		gyro: number
		accel: number
	}
	pidFrequency: number
	frequencyDivider: number
	rateFactors: number[][]
	pidConstants: number[][]
	pidConstantsNice: number[][]
	framesPerSecond: number
	isExact: boolean
	motorPoles: number
}

export type TraceInGraph = {
	path: string
	color: string
	strokeWidth: number
	minValue: number
	maxValue: number
	id: number
	unit: string
	states?: string[]
	decimals: number
	displayName: string
	overrideData?: number[]
	overrideSliceAndSkip?: number[]
}

export type FlagProps = {
	name: string
	path: string
	minValue?: number
	maxValue?: number
	rangeFn?: (file: BBLog | undefined) => { max: number; min: number }
	unit: string
	decimals?: number
	states?: string[]
	modifier?: {
		displayNameShort: string
		displayName: string
		min?: number
		max?: number
		rangeFn?: (file: BBLog | undefined) => { max: number; min: number }
		path: string
		unit?: string
		decimals?: number
		states?: string[]
	}[]
}

export type GenFlagProps = {
	replaces: string
	requires: (string | string[])[] // if its a string, that has to be in there. If its an array, one of the mentioned ones has to be in there
	exact: boolean
}

export type Command = {
	command: number
	length: number
	data: number[]
	dataStr: string
	cmdType: "request" | "response" | "error"
	flag: number
	version: number
}
