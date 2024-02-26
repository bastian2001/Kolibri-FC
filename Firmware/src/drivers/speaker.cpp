#include "global.h"

elapsedMillis soundStart;
u16 soundDuration       = 0;
u8 soundType            = 0; // 0 = stationary, 1 = sweep, 2 = rtttl
u16 sweepStartFrequency = 0;
u16 sweepEndFrequency   = 0;
u16 onTime              = 0;
u16 offTime             = 0;
u16 currentWrap         = 400;
fix32 noteFrequencies[13] =
	{
		16.35, // C0
		17.32, // C#0
		18.35, // D0
		19.45, // D#0
		20.60, // E0
		21.83, // F0
		23.12, // F#0
		24.50, // G0
		25.96, // G#0
		27.50, // A0
		29.14, // A#0
		30.87, // B0
		0      // error
};
typedef struct rtttlNote {
	u16 frequency;  // pause = 0
	u16 duration;   // milliseconds
	u8 sweepToNext; // dash after note will sweep to next, e.g. 8e6-,8d6 will sweep from 8e6 to 8d6 within "duration" milliseconds
	u8 quieter;     // 0 = normal, 1-3 = quieter, 4 = ring-tone-like e.g. 8e6$4 will play 8e6 like a ring-tone
					// quietness only sweeps statically (with the quietness level of the first note)
} RTTTLNote;
typedef struct rtttlSong {
	RTTTLNote notes[MAX_RTTTL_NOTES];
	u8 numNotes;
} RTTTLSong;
RTTTLSong songToPlay;

#define FREQ_TO_WRAP(freq) (2000000 / freq)

PIO speakerPio;
u8 speakerSm, sliceNum;
File speakerFile;
u32 speakerDataSize = 0;
u32 speakerCounter  = 0;
dma_channel_config speakerDmaAConfig, speakerDmaBConfig;
u8 speakerDmaAChan, speakerDmaBChan;
volatile u8 soundState = 1; // 1: playing PWM, 2: a needs to be filled, 4: b needs to be filled
// 1KiB buffer each, thus worst case the speaker has a buffer of 1024 samples / 44.1kHz = 23ms
u8 speakerChanAData[1 << SPEAKER_SIZE_POWER] __attribute__((aligned(1 << SPEAKER_SIZE_POWER))) = {0};
u8 speakerChanBData[1 << SPEAKER_SIZE_POWER] __attribute__((aligned(1 << SPEAKER_SIZE_POWER))) = {0};

void dmaIrqHandler() {
	u32 interrupts = dma_hw->intr;
	if (interrupts & (1u << speakerDmaAChan)) {
		dma_hw->ints0 = 1u << speakerDmaAChan;
		soundState |= 0b10;
	}
	if (interrupts & (1u << speakerDmaBChan)) {
		dma_hw->ints0 = 1u << speakerDmaBChan;
		soundState |= 0b100;
	}
}

void initSpeaker() {
	speakerPio      = pio0;
	uint offset     = pio_add_program(speakerPio, &speaker8bit_program);
	speakerSm       = pio_claim_unused_sm(speakerPio, true);
	pio_sm_config c = speaker8bit_program_get_default_config(offset);
	pio_gpio_init(speakerPio, PIN_SPEAKER);
	sm_config_set_set_pins(&c, PIN_SPEAKER, 1);
	sm_config_set_out_pins(&c, PIN_SPEAKER, 1);
	sm_config_set_in_pins(&c, PIN_SPEAKER);
	sm_config_set_jmp_pin(&c, PIN_SPEAKER);
	sm_config_set_out_shift(&c, false, false, 32);
	sm_config_set_in_shift(&c, false, false, 32);
	pio_sm_set_consecutive_pindirs(speakerPio, speakerSm, PIN_SPEAKER, 1, true);
	gpio_set_drive_strength(PIN_SPEAKER, GPIO_DRIVE_STRENGTH_2MA);
	sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
	pio_sm_init(speakerPio, speakerSm, offset, &c);
	pio_sm_set_enabled(speakerPio, speakerSm, true);
	pio_sm_put(speakerPio, speakerSm, 0);
	pio_sm_set_clkdiv_int_frac(speakerPio, speakerSm, 11, 148); // 44.1kHz * 4
	speakerDmaAChan   = dma_claim_unused_channel(true);
	speakerDmaAConfig = dma_channel_get_default_config(speakerDmaAChan);
	speakerDmaBChan   = dma_claim_unused_channel(true);
	speakerDmaBConfig = dma_channel_get_default_config(speakerDmaBChan);
	channel_config_set_read_increment(&speakerDmaAConfig, true);
	channel_config_set_write_increment(&speakerDmaAConfig, false);
	channel_config_set_dreq(&speakerDmaAConfig, pio_get_dreq(speakerPio, speakerSm, true));
	channel_config_set_transfer_data_size(&speakerDmaAConfig, DMA_SIZE_32);
	channel_config_set_chain_to(&speakerDmaAConfig, speakerDmaBChan);
	channel_config_set_ring(&speakerDmaAConfig, false, SPEAKER_SIZE_POWER);
	dma_channel_set_irq0_enabled(speakerDmaAChan, true);
	dma_channel_set_read_addr(speakerDmaAChan, speakerChanAData, true);
	dma_channel_set_write_addr(speakerDmaAChan, &speakerPio->txf[speakerSm], false);
	// identical setup to a, just different buffer
	channel_config_set_read_increment(&speakerDmaBConfig, true);
	channel_config_set_write_increment(&speakerDmaBConfig, false);
	channel_config_set_dreq(&speakerDmaBConfig, pio_get_dreq(speakerPio, speakerSm, true));
	channel_config_set_transfer_data_size(&speakerDmaBConfig, DMA_SIZE_32);
	channel_config_set_chain_to(&speakerDmaBConfig, speakerDmaAChan);
	channel_config_set_ring(&speakerDmaBConfig, false, SPEAKER_SIZE_POWER);
	dma_channel_set_irq0_enabled(speakerDmaBChan, true);
	dma_channel_set_read_addr(speakerDmaBChan, speakerChanBData, true);
	dma_channel_set_write_addr(speakerDmaBChan, &speakerPio->txf[speakerSm], false);
	if (playWav("start.wav")) {
		soundState = 0b110;
	} else {
		soundState = 0b001;
		makeSound(1000, 100, 100, 0);
	}
	irq_set_exclusive_handler(DMA_IRQ_0, dmaIrqHandler);
	irq_set_enabled(DMA_IRQ_0, true);
	soundState = 0b110;

	sliceNum = pwm_gpio_to_slice_num(PIN_SPEAKER);
	pwm_set_clkdiv_int_frac(sliceNum, 132, 0);
	pwm_set_wrap(sliceNum, FREQ_TO_WRAP(1000));
	pwm_set_gpio_level(PIN_SPEAKER, 0);
	pwm_set_enabled(sliceNum, true);
}

u8 beeperOn         = 0;
u32 totalBytes      = 0;
u8 finishedChannels = 0b00;
u8 startSpeakerFile = true;
void speakerLoop() {
	if (soundState & 0b110) {
		u32 bytesRead = 0;
		u8 pState     = soundState;
		if (soundState & 0b10) {
			soundState &= 0b1101;
			bytesRead = speakerFile.read(speakerChanAData, 1 << SPEAKER_SIZE_POWER);
			dma_channel_set_trans_count(speakerDmaAChan, bytesRead / 4, false);
		}
		if (soundState & 0b100) {
			soundState &= 0b1011;
			bytesRead = speakerFile.read(speakerChanBData, 1 << SPEAKER_SIZE_POWER);
			dma_channel_set_trans_count(speakerDmaBChan, bytesRead / 4, false);
		}
		if (bytesRead <= 0) {
			if (pState & 0b10) {
				channel_config_set_enable(&speakerDmaAConfig, false);
				dma_channel_set_config(speakerDmaAChan, &speakerDmaAConfig, false);
				finishedChannels |= 0b01;
			}
			if (pState & 0b100) {
				channel_config_set_enable(&speakerDmaBConfig, false);
				dma_channel_set_config(speakerDmaBChan, &speakerDmaBConfig, false);
				finishedChannels |= 0b10;
			}
			if (finishedChannels == 0b11) {
				speakerFile.close();
				gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);
				soundState = 1; // done playing, switch to PWM
				return;
			}
		}
		if (startSpeakerFile) {
			dma_channel_start(speakerDmaAChan);
			finishedChannels = 0b00;
			startSpeakerFile = false;
		}
		return;
	} else if (soundState != 1) {
		return;
	}

	elapsedMicros taskTimer = 0;
	tasks[TASK_SPEAKER].runCounter++;
	if (!beeperOn && ((ELRS->channels[9] > 1500 && ELRS->isLinkUp) || (ELRS->sinceLastRCMessage > 240000000 && ELRS->rcMsgCount > 50))) {
		beeperOn = true;
		makeSweepSound(1000, 5000, 65535, 600, 0);
	} else if (beeperOn && (ELRS->channels[9] <= 1500 && ELRS->isLinkUp)) {
		beeperOn = false;
		stopSound();
	}
	if (soundDuration > 0) {
		u32 sinceStart = soundStart;
		if (soundDuration != 65535 && sinceStart > soundDuration) {
			stopSound();
		} else if (soundType == 1) {
			int thisCycle = sinceStart % (onTime + offTime);
			if (thisCycle > onTime) {
				pwm_set_gpio_level(PIN_SPEAKER, 0);
			} else {
				u32 thisFreq = sweepStartFrequency + ((sweepEndFrequency - sweepStartFrequency) * thisCycle) / onTime;
				currentWrap  = FREQ_TO_WRAP(thisFreq);
				pwm_set_wrap(sliceNum, currentWrap);
				pwm_set_gpio_level(PIN_SPEAKER, currentWrap >> 1);
			}
		} else if (soundType == 2) {
			int thisCycle = sinceStart;
			int noteIndex = 0;
			while (thisCycle > songToPlay.notes[noteIndex].duration && noteIndex < songToPlay.numNotes && noteIndex < MAX_RTTTL_NOTES) {
				thisCycle -= songToPlay.notes[noteIndex].duration;
				noteIndex++;
			}
			if (noteIndex >= songToPlay.numNotes || noteIndex >= MAX_RTTTL_NOTES) {
				stopSound();
			} else {
				if (songToPlay.notes[noteIndex].frequency == 0) {
					pwm_set_gpio_level(PIN_SPEAKER, 0);
				} else {
					if (songToPlay.notes[noteIndex].sweepToNext) {
						u32 thisFreq = songToPlay.notes[noteIndex].frequency + ((songToPlay.notes[noteIndex + 1].frequency - songToPlay.notes[noteIndex].frequency) * thisCycle) / songToPlay.notes[noteIndex].duration;
						currentWrap  = FREQ_TO_WRAP(thisFreq);
					} else {
						currentWrap = FREQ_TO_WRAP(songToPlay.notes[noteIndex].frequency);
					}
					pwm_set_wrap(sliceNum, currentWrap);
					int level = currentWrap >> 1;
					gpio_set_drive_strength(PIN_SPEAKER, GPIO_DRIVE_STRENGTH_2MA);
					switch (songToPlay.notes[noteIndex].quieter) {
					case 1:
						level = level >> 4;
						break;
					case 2:
						level = level >> 6;
						break;
					case 3:
						level = level >> 7;
						break;
					case 4:
						level = level >> 2;
					}
					pwm_set_gpio_level(PIN_SPEAKER, level);
				}
			}
		} else if (soundType == 0) {
			u32 thisCycle = sinceStart % (onTime + offTime);
			if (thisCycle > onTime) {
				pwm_set_gpio_level(PIN_SPEAKER, 0);
			} else {
				pwm_set_gpio_level(PIN_SPEAKER, currentWrap >> 1);
			}
		}
	}
	u32 duration = taskTimer;
	tasks[TASK_SPEAKER].totalDuration += duration;
	if (duration < tasks[TASK_SPEAKER].minDuration) {
		tasks[TASK_SPEAKER].minDuration = duration;
	}
	if (duration > tasks[TASK_SPEAKER].maxDuration) {
		tasks[TASK_SPEAKER].maxDuration = duration;
	}
}

bool playWav(const char *filename) {
	if (soundState != 1) {
		return false;
	}
	if (fsReady && (speakerFile = SDFS.open(filename, "r")) && speakerFile.size() > 1002) {
		while (speakerFile.position() < 1000) { // skip wav header
			if (speakerFile.read() == 'd' && speakerFile.read() == 'a' && speakerFile.read() == 't' && speakerFile.read() == 'a') {
				speakerCounter = speakerFile.position() + 4;
				break;
			}
		}
		speakerDataSize = speakerFile.size() - speakerCounter;
		if (speakerDataSize) {
			channel_config_set_enable(&speakerDmaAConfig, true);
			dma_channel_set_config(speakerDmaAChan, &speakerDmaAConfig, false);
			channel_config_set_enable(&speakerDmaBConfig, true);
			dma_channel_set_config(speakerDmaBChan, &speakerDmaBConfig, false);
			soundState = 0b110;
			gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PIO0);
			startSpeakerFile = true;
			return true;
		}
		speakerFile.close();
		return false;
	} else
		return false;
}

void makeSound(u16 frequency, u16 duration, u16 tOnMs, u16 tOffMs) {
	soundType   = 0;
	currentWrap = FREQ_TO_WRAP(frequency);
	pwm_set_wrap(sliceNum, currentWrap);
	onTime        = tOnMs;
	offTime       = tOffMs;
	soundDuration = duration;
	soundStart    = 0;
}

void stopSound() {
	soundDuration = 0;
	pwm_set_gpio_level(PIN_SPEAKER, 0);
}

// sweep from startFrequency to endFrequency over tOnMs, then stop for tOffMs, repeat for duration
void makeSweepSound(u16 startFrequency, u16 endFrequency, u16 duration, u16 tOnMs, u16 tOffMs) {
	soundType           = 1;
	sweepStartFrequency = startFrequency;
	sweepEndFrequency   = endFrequency;
	onTime              = tOnMs;
	offTime             = tOffMs;
	soundDuration       = duration;
	soundStart          = 0;
}

int parseInt(const char *str, int *index, int defaultValue = 0) {
	int result = 0;
	int len    = 0;
	while (str[len + *index] >= '0' && str[len + *index] <= '9' && len < 9) {
		result *= 10;
		result += str[len + *index] - '0';
		len++;
	}
	if (len == 0)
		return defaultValue;
	*index += len;
	return result;
}

void makeRtttlSound(const char *song) {
	int colonCount    = 0;
	int colon[2]      = {-1, -1};
	int totalDuration = 0;
	for (int i = 0; i < MAX_RTTTL_TEXT_LENGTH; i++) {
		if (song[i] == ':') {
			colon[colonCount] = i;
			colonCount++;
		}
		if (colonCount == 2) {
			break;
		}
		if (song[i] == '\0')
			break;
	}
	int d, o, b, notesStart;
	if (colonCount == 0) {
		notesStart = 0;
		d          = 4;
		o          = 5;
		b          = 125;
	} else if (colonCount >= 1) {
		int start = 0, end = 0;
		if (colonCount == 1) {
			start = colon[0];
			end   = 0;
		} else if (colonCount == 2) {
			start = colon[1];
			end   = colon[0];
		}
		notesStart = start + 1;
		// find index of 'd' in song before colon
		int i = start - 3;
		while (song[i] != 'd' && i >= end) {
			i--;
		}
		if (i < end) {
			d = 4;
		} else {
			i += 2;
			d = parseInt(song, &i);
		}
		// find index of 'o' in song before colon
		i = start - 3;
		while (song[i] != 'o' && i >= end) {
			i--;
		}
		if (i < end) {
			o = 5;
		} else {
			i += 2;
			o = parseInt(song, &i);
		}
		// find index of 'b' in song before colon
		i = start - 3;
		while (song[i] != 'b' && i >= end) {
			i--;
		}
		if (i < end) {
			b = 125;
		} else {
			i += 2;
			b = parseInt(song, &i);
		}
	}
	int noteIndex = 0;
	for (int i = notesStart; i < MAX_RTTTL_TEXT_LENGTH;) {
		if (song[i] == '\0')
			break;
		if (song[i] == ',') {
			i++;
			continue;
		}
		if (noteIndex >= MAX_RTTTL_NOTES)
			break;
		int noteLength = parseInt(song, &i, -1);
		if (noteLength == -1)
			noteLength = d;
		if (noteLength)
			songToPlay.notes[noteIndex].duration = 240000 / b / noteLength;
		else
			songToPlay.notes[noteIndex].duration = 0;
		if (song[i] == 'p') {
			songToPlay.notes[noteIndex].frequency   = 0;
			songToPlay.notes[noteIndex].sweepToNext = 0;
			i++;
		} else {
			int note      = 0; // 0 = C, 1 = C#...
			char noteChar = song[i++];
			switch (noteChar) {
			case 'c':
			case 'C':
				note = 0;
				break;
			case 'd':
			case 'D':
				note = 2;
				break;
			case 'e':
			case 'E':
				note = 4;
				break;
			case 'f':
			case 'F':
				note = 5;
				break;
			case 'g':
			case 'G':
				note = 7;
				break;
			case 'a':
			case 'A':
				note = 9;
				break;
			case 'b':
			case 'B':
				note = 11;
				break;
			default:
				note = 12;
			}
			if (song[i] == '#') {
				note++;
				i++;
			}
			int octave = parseInt(song, &i);
			if (octave == 0)
				octave = o;
			songToPlay.notes[noteIndex].frequency = (noteFrequencies[note] << octave).getInt();
			if (song[i] == '.') {
				songToPlay.notes[noteIndex].duration = songToPlay.notes[noteIndex].duration * 3 / 2;
				i++;
			}
			if (song[i] == '-') {
				songToPlay.notes[noteIndex].sweepToNext = 1;
				i++;
			} else {
				songToPlay.notes[noteIndex].sweepToNext = 0;
			}
			if (song[i] == '$') {
				i++;
				songToPlay.notes[noteIndex].quieter = parseInt(song, &i, 0);
			}
		}
		totalDuration += songToPlay.notes[noteIndex].duration;
		songToPlay.numNotes = noteIndex + 1;
		noteIndex++;
		// go forward to next ,
		while (song[i] != ',' && song[i] != '\0' && i++ < MAX_RTTTL_TEXT_LENGTH) {
		}
	}
	songToPlay.notes[songToPlay.numNotes - 1].sweepToNext = 0;

	soundDuration = totalDuration;
	soundStart    = 0;
	soundType     = 2;
}
