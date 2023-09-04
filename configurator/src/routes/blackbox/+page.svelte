<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { port, type Command, ConfigCmd } from '../../stores';

	const ACC_RANGES = [2, 4, 8, 16];
	const GYRO_RANGES = [2000, 1000, 500, 250, 125];

	let logNums = [] as { text: string; num: number }[];
	type BBLog = {
		frameCount: number;
		flags: string[];
		frames: LogFrame[];
		version: number[];
		startTime: number;
		ranges: {
			gyro: number;
			accel: number;
		};
		pidFrequency: number;
		frequencyDivider: number;
		rateFactors: number[][];
		pidConstants: number[][];
		framesPerSecond: number;
	};

	let binFile = [] as number[];
	let binFileNumber = -1;
	let receivedChunks = [] as boolean[];
	let totalChunks = 0;
	let resolveWhenReady = (log: BBLog) => {};
	let rejectWrongFile = (_: string) => {};

	type LogFrame = {
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
	};

	const BB_FLAG_NAMES = [
		'LOG_ROLL_ELRS_RAW',
		'LOG_PITCH_ELRS_RAW',
		'LOG_THROTTLE_ELRS_RAW',
		'LOG_YAW_ELRS_RAW',
		'LOG_ROLL_SETPOINT',
		'LOG_PITCH_SETPOINT',
		'LOG_THROTTLE_SETPOINT',
		'LOG_YAW_SETPOINT',
		'LOG_ROLL_GYRO_RAW',
		'LOG_PITCH_GYRO_RAW',
		'LOG_YAW_GYRO_RAW',
		'LOG_ROLL_PID_P',
		'LOG_ROLL_PID_I',
		'LOG_ROLL_PID_D',
		'LOG_ROLL_PID_FF',
		'LOG_ROLL_PID_S',
		'LOG_PITCH_PID_P',
		'LOG_PITCH_PID_I',
		'LOG_PITCH_PID_D',
		'LOG_PITCH_PID_FF',
		'LOG_PITCH_PID_S',
		'LOG_YAW_PID_P',
		'LOG_YAW_PID_I',
		'LOG_YAW_PID_D',
		'LOG_YAW_PID_FF',
		'LOG_YAW_PID_S',
		'LOG_MOTOR_OUTPUTS',
		'LOG_ALTITUDE'
	];

	$: handleCommand($port);

	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.BB_FILE_LIST | 0x4000:
				logNums = command.data.map((n) => ({ text: '', num: n }));
				logInfoPosition = 0;
				logInfoInterval = setInterval(getLogInfo, 500);
				break;
			case ConfigCmd.BB_FILE_INFO | 0x4000:
				processLogInfo(command.data);
				break;
			case ConfigCmd.BB_FILE_DOWNLOAD | 0x4000:
				handleFileChunk(command.data);
		}
	}

	function leBytesToInt(bytes: number[]) {
		let value = 0;
		for (let i = 0; i < bytes.length; i++) {
			value += bytes[i] * Math.pow(256, i);
		}
		return value;
	}

	function handleFileChunk(data: number[]) {
		//1027 data bytes per packet
		//first byte is file number
		//second and third are chunk number
		//up to 1024 bytes of binary file data
		//last packet has the chunk number set to 0xFFFF, and then two bytes that indicate the number of total chunks
		if (binFileNumber !== data[0]) {
			binFileNumber = data[0];
			binFile = [];
			receivedChunks = [];
			totalChunks = 0;
		}
		const chunkNum = data[1] + data[2] * 256;
		if (chunkNum === 0xffff) {
			totalChunks = data[3] + data[4] * 256;
		} else {
			const chunkSize = data.length - 3;
			const chunk = data.slice(3, 3 + chunkSize);
			receivedChunks[chunkNum] = true;
			for (let i = chunkNum * 1024; i < chunkNum * 1024 + chunkSize; i++) {
				binFile[i] = chunk[i - chunkNum * 1024];
			}
		}
		if (receivedChunks.length === totalChunks) {
			for (let i = 0; i < totalChunks; i++) {
				if (!receivedChunks[i]) return;
			}
			const header = binFile.slice(0, 166);
			const data = binFile.slice(166);
			const magic = leBytesToInt(header.slice(0, 4));
			if (magic !== 0x99a12720) {
				rejectWrongFile('Wrong magic number');
				return;
			}
			const version = header.slice(4, 7);
			const startTime = leBytesToInt(header.slice(7, 11));
			const pidFreq = 3200 / (1 + header[11]);
			const freqDiv = header[12];
			const rangeByte = header[13];
			const ranges = {
				gyro: GYRO_RANGES[(rangeByte >> 2) & 0b111],
				accel: ACC_RANGES[rangeByte & 0b11]
			};
			const rateFactors = [[], [], [], [], []] as number[][];
			const rfBytes = header.slice(14, 74);
			for (let i = 0; i < 5; i++)
				for (let j = 0; j < 3; j++)
					rateFactors[i][j] =
						leBytesToInt(rfBytes.slice(i * 12 + j * 4, i * 12 + j * 4 + 4)) / 65536;
			const pidConstants = [[], [], []] as number[][];
			const pcBytes = header.slice(74, 158);
			for (let i = 0; i < 3; i++)
				for (let j = 0; j < 7; j++)
					pidConstants[i][j] =
						leBytesToInt(pcBytes.slice(i * 28 + j * 4, i * 28 + j * 4 + 4)) / 65536;
			const flagsLow = leBytesToInt(header.slice(158, 162));
			const flagsHigh = leBytesToInt(header.slice(162, 166));
			const flags = [] as string[];
			let frameSize = 0;
			const offsets = {} as { [key: string]: number };
			for (let i = 0; i < 32 && BB_FLAG_NAMES.length > i; i++) {
				const flagIsSet = flagsLow & (1 << i);
				if (flagIsSet) {
					flags.push(BB_FLAG_NAMES[i]);
					offsets[BB_FLAG_NAMES[i]] = frameSize;
					frameSize += i == 26 ? 6 : 2;
				}
			}
			for (let i = 0; i < 32 && BB_FLAG_NAMES.length > i + 32; i++) {
				const flagIsSet = flagsHigh & (1 << i);
				if (flagIsSet) flags.push(BB_FLAG_NAMES[i + 32]);
			}
			const framesPerSecond = pidFreq / freqDiv;
			const frames = data.length / frameSize;
			const log = [] as LogFrame[];
			for (let i = 0; i < data.length; i += frameSize) {
				const frame = {
					elrs: {},
					setpoint: {},
					gyro: {},
					pid: { roll: {}, pitch: {}, yaw: {} },
					motors: {}
				} as LogFrame;
				if (flags.includes('LOG_ROLL_ELRS_RAW'))
					frame.elrs.roll = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_ELRS_RAW'], i + offsets['LOG_ROLL_ELRS_RAW'] + 2)
					);
				if (flags.includes('LOG_PITCH_ELRS_RAW'))
					frame.elrs.pitch = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_ELRS_RAW'], i + offsets['LOG_PITCH_ELRS_RAW'] + 2)
					);
				if (flags.includes('LOG_THROTTLE_ELRS_RAW'))
					frame.elrs.throttle = leBytesToInt(
						data.slice(
							i + offsets['LOG_THROTTLE_ELRS_RAW'],
							i + offsets['LOG_THROTTLE_ELRS_RAW'] + 2
						)
					);
				if (flags.includes('LOG_YAW_ELRS_RAW'))
					frame.elrs.yaw = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_ELRS_RAW'], i + offsets['LOG_YAW_ELRS_RAW'] + 2)
					);
				if (flags.includes('LOG_ROLL_SETPOINT'))
					frame.setpoint.roll =
						leBytesToInt(
							data.slice(i + offsets['LOG_ROLL_SETPOINT'], i + offsets['LOG_ROLL_SETPOINT'] + 2)
						) / 16; // data is 12.4 fixed point
				if (flags.includes('LOG_PITCH_SETPOINT'))
					frame.setpoint.pitch =
						leBytesToInt(
							data.slice(i + offsets['LOG_PITCH_SETPOINT'], i + offsets['LOG_PITCH_SETPOINT'] + 2)
						) / 16;
				if (flags.includes('LOG_THROTTLE_SETPOINT'))
					frame.setpoint.throttle = leBytesToInt(
						data.slice(
							i + offsets['LOG_THROTTLE_SETPOINT'],
							i + offsets['LOG_THROTTLE_SETPOINT'] + 2
						)
					);
				if (flags.includes('LOG_YAW_SETPOINT'))
					frame.setpoint.yaw =
						leBytesToInt(
							data.slice(i + offsets['LOG_YAW_SETPOINT'], i + offsets['LOG_YAW_SETPOINT'] + 2)
						) / 16;
				if (flags.includes('LOG_ROLL_GYRO_RAW'))
					frame.gyro.roll =
						(leBytesToInt(
							data.slice(i + offsets['LOG_ROLL_GYRO_RAW'], i + offsets['LOG_ROLL_GYRO_RAW'] + 2)
						) /
							32768) *
						ranges.gyro;
				if (flags.includes('LOG_PITCH_GYRO_RAW'))
					frame.gyro.pitch =
						(leBytesToInt(
							data.slice(i + offsets['LOG_PITCH_GYRO_RAW'], i + offsets['LOG_PITCH_GYRO_RAW'] + 2)
						) /
							32768) *
						ranges.gyro;
				if (flags.includes('LOG_YAW_GYRO_RAW'))
					frame.gyro.yaw =
						(leBytesToInt(
							data.slice(i + offsets['LOG_YAW_GYRO_RAW'], i + offsets['LOG_YAW_GYRO_RAW'] + 2)
						) /
							32768) *
						ranges.gyro;
				if (flags.includes('LOG_ROLL_PID_P'))
					frame.pid.roll.p = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_P'], i + offsets['LOG_ROLL_PID_P'] + 2)
					);
				if (flags.includes('LOG_ROLL_PID_I'))
					frame.pid.roll.i = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_I'], i + offsets['LOG_ROLL_PID_I'] + 2)
					);
				if (flags.includes('LOG_ROLL_PID_D'))
					frame.pid.roll.d = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_D'], i + offsets['LOG_ROLL_PID_D'] + 2)
					);
				if (flags.includes('LOG_ROLL_PID_FF'))
					frame.pid.roll.ff = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_FF'], i + offsets['LOG_ROLL_PID_FF'] + 2)
					);
				if (flags.includes('LOG_ROLL_PID_S'))
					frame.pid.roll.s = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_S'], i + offsets['LOG_ROLL_PID_S'] + 2)
					);
				if (flags.includes('LOG_PITCH_PID_P'))
					frame.pid.pitch.p = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_P'], i + offsets['LOG_PITCH_PID_P'] + 2)
					);
				if (flags.includes('LOG_PITCH_PID_I'))
					frame.pid.pitch.i = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_I'], i + offsets['LOG_PITCH_PID_I'] + 2)
					);
				if (flags.includes('LOG_PITCH_PID_D'))
					frame.pid.pitch.d = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_D'], i + offsets['LOG_PITCH_PID_D'] + 2)
					);
				if (flags.includes('LOG_PITCH_PID_FF'))
					frame.pid.pitch.ff = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_FF'], i + offsets['LOG_PITCH_PID_FF'] + 2)
					);
				if (flags.includes('LOG_PITCH_PID_S'))
					frame.pid.pitch.s = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_S'], i + offsets['LOG_PITCH_PID_S'] + 2)
					);
				if (flags.includes('LOG_YAW_PID_P'))
					frame.pid.yaw.p = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_P'], i + offsets['LOG_YAW_PID_P'] + 2)
					);
				if (flags.includes('LOG_YAW_PID_I'))
					frame.pid.yaw.i = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_I'], i + offsets['LOG_YAW_PID_I'] + 2)
					);
				if (flags.includes('LOG_YAW_PID_D'))
					frame.pid.yaw.d = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_D'], i + offsets['LOG_YAW_PID_D'] + 2)
					);
				if (flags.includes('LOG_YAW_PID_FF'))
					frame.pid.yaw.ff = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_FF'], i + offsets['LOG_YAW_PID_FF'] + 2)
					);
				if (flags.includes('LOG_YAW_PID_S'))
					frame.pid.yaw.s = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_S'], i + offsets['LOG_YAW_PID_S'] + 2)
					);
				if (flags.includes('LOG_MOTOR_OUTPUTS')) {
					const throttleBytes = data.slice(
						i + offsets['LOG_MOTOR_OUTPUTS'],
						i + offsets['LOG_MOTOR_OUTPUTS'] + 6
					);
					const motors01 = leBytesToInt(throttleBytes.slice(0, 3).reverse());
					const motors23 = leBytesToInt(throttleBytes.slice(3, 6).reverse());
					frame.motors.rr = motors01 >> 12;
					frame.motors.fr = motors01 & 0xfff;
					frame.motors.rl = motors23 >> 12;
					frame.motors.fl = motors23 & 0xfff;
				}
				if (flags.includes('LOG_ALTITUDE'))
					frame.altitude = leBytesToInt(
						data.slice(i + offsets['LOG_ALTITUDE'], i + offsets['LOG_ALTITUDE'] + 2)
					);
				log.push(frame);
			}
			const result: BBLog = {
				frameCount: frames,
				flags,
				frames: log,
				version,
				startTime,
				ranges,
				pidFrequency: pidFreq,
				frequencyDivider: freqDiv,
				rateFactors,
				pidConstants,
				framesPerSecond
			};
			resolveWhenReady(result);
		}
	}

	let logInfoPosition = 0;
	let logInfoInterval = -1;
	let selected = 0;
	function getLogInfo() {
		const infoNums = [] as number[];
		for (let i = 0; i < 10; i++) {
			if (logInfoPosition >= logNums.length) {
				clearInterval(logInfoInterval);
				break;
			}
			infoNums[i] = logNums[logInfoPosition++].num;
		}
		if (infoNums.length == 0) return;
		let checksum = 0;
		for (let i = 0; i < infoNums.length; i++) checksum ^= infoNums[i];
		checksum ^= 0x06;
		checksum ^= infoNums.length;
		port.sendCommand(0x06, infoNums);
	}

	function processLogInfo(data: number[]) {
		/* data of response (repeat 22 bytes for each log file)
		 * 0. file number
		 * 1-4. file size in bytes
		 * 5-7. version of bb file format
		 * 8-11: time of recording start
		 * 12. byte that indicates PID frequency
		 * 13. byte that indicates frequency divider
		 * 14-21: recording flags
		 */
		for (let i = 0; i < data.length; i += 22) {
			const fileNum = data[i];
			const fileSize =
				data[i + 1] + data[i + 2] * 256 + data[i + 3] * 256 * 256 + data[i + 4] * 256 * 256 * 256;
			const bbVersion = data[i + 5] + data[i + 6] * 256 + data[i + 7] * 256 * 256;
			const startTime =
				data[i + 8] + data[i + 9] * 256 + data[i + 10] * 256 * 256 + data[i + 11] * 256 * 256 * 256;
			const pidFreq = 3200 / (data[i + 12] + 1);
			const freqDiv = data[i + 13];
			const flags = data.slice(i + 14, i + 22);
			if (bbVersion !== 0) continue;
			const framesPerSecond = pidFreq / freqDiv;
			const dataBytes = fileSize - 166;
			let frameSize = 0;
			for (let j = 0; j < 64; j++) {
				//check flags
				// flag 26 (motors) has 6 bytes per frame, all others only 2
				const byteNum = Math.floor(j / 8);
				const bitNum = j % 8;
				const flagIsSet = flags[byteNum] & (1 << bitNum);
				if (!flagIsSet) continue;
				if (j == 26) frameSize += 6;
				else frameSize += 2;
			}
			const frames = dataBytes / frameSize;
			//append duration of log file to logNums
			const index = logNums.findIndex((n) => n.num == fileNum);
			if (index == -1) continue;
			const duration = Math.round(frames / framesPerSecond);
			logNums[index].text = `${logNums[index].num} - ${duration}s`;
			selected = fileNum;
		}
	}

	function deleteLog() {
		const logNum = logNums[0].num as number;
		port.sendCommand(ConfigCmd.BB_FILE_DELETE, [logNum]);
	}
	function openLog() {
		getLog(selected).then((data) => {
			console.log(data);
		});
	}
	function downloadLog() {}
	function getLog(num: number): Promise<BBLog> {
		port.sendCommand(ConfigCmd.BB_FILE_DOWNLOAD, [num]);
		return new Promise((resolve, reject) => {
			resolveWhenReady = resolve;
			rejectWrongFile = reject;
		});
	}
	function formatBB() {
		port.sendCommand(ConfigCmd.BB_FORMAT);
	}

	onMount(() => {
		port.sendCommand(ConfigCmd.BB_FILE_LIST).catch(console.error);
	});
	onDestroy(() => {});
</script>

<div class="blackbox">
	<div class="selector">
		<select bind:value={selected}>
			{#each logNums as log}
				<option value={log?.num}>{log?.text || log?.num}</option>
			{/each}
		</select>
		<button on:click={() => openLog()}>Open</button>
		<button on:click={() => downloadLog()}>Download</button>
		<button on:click={() => deleteLog()}>Delete</button>
		<button on:click={() => formatBB()}>Format</button>
	</div>
	<div class="dataViewer">Data Viewer</div>
</div>

<style>
	.blackbox {
		width: 100%;
		height: 100%;
	}

	.selector {
		padding: 0.5rem;
	}

	.selector {
		display: flex;
		flex-direction: row;
		align-items: center;
	}

	.selector select {
		width: 8rem;
		background-color: transparent;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		padding: 3px 6px;
		color: var(--text-color);
		outline: none;
	}
	.selector select option {
		color: #888;
	}

	button {
		background-color: transparent;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		padding: 4px 8px;
		color: var(--text-color);
		outline: none;
		margin-left: 0.8rem;
	}
</style>
