
#if BLACKBOX_STORAGE == FLASH_BB

#include "global.h"
#include "pioasm/extended_spi.pio.h"

enum FlashRegisters {
	FLASH_CMD_RESET = 0xFF,
	FLASH_CMD_GET_FEATURE = 0x0F,
	FLASH_CMD_SET_FEATURE = 0x1F,
	FLASH_CMD_READ_ID = 0x9F,
	FLASH_CMD_PAGE_READ = 0x13,
	// FLASH_CMD_READ_PAGE_CACHE_RANDOM = 0x30,
	// FLASH_CMD_READ_PAGE_CACHE_LAST = 0x3F,
	FLASH_CMD_READ_FROM_CACHE_X1 = 0x03,
	FLASH_CMD_READ_FROM_CACHE_X2 = 0x3B,
	// FLASH_CMD_READ_FROM_CACHE_X4 = 0x6B,
	FLASH_CMD_READ_FROM_CACHE_DUAL_IO = 0xBB,
	// FLASH_CMD_READ_FROM_CACHE_QUAD_IO = 0xEB,
	FLASH_CMD_WRITE_ENABLE = 0x06,
	FLASH_CMD_WRITE_DISABLE = 0x04,
	FLASH_CMD_BLOCK_ERASE = 0xD8,
	FLASH_CMD_PROGRAM_EXECUTE = 0x10,
	FLASH_CMD_PROGRAM_LOAD_X1 = 0x02,
	// FLASH_CMD_PROGRAM_LOAD_X4 = 0x32,
	FLASH_CMD_PROGRAM_LOAD_RANDOM_DATA_X1 = 0x84,
	// FLASH_CMD_PROGRAM_LOAD_RANDOM_DATA_X4 = 0x34,
	// FLASH_CMD_PERMANENT_BLOCK_LOCK_PROTECTION = 0x2C,
};

// sends/receives a simple RX/TX byte via SPI to the blackbox flash chip
u8 Fckafd::singleSpiTransfer(u8 txByte) {
	pio_sm_clear_fifos(PIO_EXT_SPI_BB, blackboxSm);
	pio_sm_put_blocking(PIO_EXT_SPI_BB, blackboxSm, ((u32)txByte) << 24);
	return pio_sm_get_blocking(PIO_EXT_SPI_BB, blackboxSm);
}
void Fckafd::burstSpiRead(u16 len, u8 *dst) {
	if (dst == nullptr) return;
	u8 *dummy = (u8 *)malloc(len);
	pio_sm_clear_fifos(PIO_EXT_SPI_BB, blackboxSm);
	dma_channel_abort(dmaTxChannel);
	dma_channel_abort(dmaRxChannel);
	while (dma_channel_is_busy(dmaTxChannel) || dma_channel_is_busy(dmaRxChannel) || dma_hw->abort & (1u << dmaTxChannel | 1u << dmaRxChannel)) {
		// wait until busy is deasserted and abort is cleared (12.6.8.3 and E5)
		tight_loop_contents();
	}

	dma_channel_set_write_addr(dmaRxChannel, dst, false);
	dma_channel_set_transfer_count(dmaRxChannel, len, false);
	dma_channel_set_read_addr(dmaTxChannel, dummy, false);
	dma_channel_set_transfer_count(dmaTxChannel, len, false);
	dma_start_channel_mask((1u << dmaTxChannel) | (1u << dmaRxChannel));

	while (dma_channel_is_busy(dmaTxChannel) || dma_channel_is_busy(dmaRxChannel)) {
		tight_loop_contents();
	}
	free(dummy);
}
void Fckafd::burstSpiWrite(u16 len, const u8 *src) {
	if (src == nullptr) return;
	u8 *dummy = (u8 *)malloc(len);
	pio_sm_clear_fifos(PIO_EXT_SPI_BB, blackboxSm);
	dma_channel_abort(dmaTxChannel);
	dma_channel_abort(dmaRxChannel);
	while (dma_channel_is_busy(dmaTxChannel) || dma_channel_is_busy(dmaRxChannel) || dma_hw->abort & (1u << dmaTxChannel | 1u << dmaRxChannel)) {
		// wait until busy is deasserted and abort is cleared (12.6.8.3 and E5)
		tight_loop_contents();
	}

	dma_channel_set_transfer_count(dmaRxChannel, len, false);
	dma_channel_set_write_addr(dmaRxChannel, dummy, false);
	dma_channel_set_transfer_count(dmaTxChannel, len, false);
	dma_channel_set_read_addr(dmaTxChannel, src, false);
	dma_start_channel_mask((1u << dmaTxChannel) | (1u << dmaRxChannel));

	while (dma_channel_is_busy(dmaRxChannel) || dma_channel_is_busy(dmaTxChannel)) {
		tight_loop_contents();
	}
	free(dummy);
}

bool Fckafd::checkReadId() {
	gpio_put(PIN_FLASH_CS, false);
	singleSpiTransfer(FLASH_CMD_READ_ID);
	singleSpiTransfer(); // dummy byte
	u8 read0 = singleSpiTransfer();
	u8 read1 = singleSpiTransfer();
	gpio_put(PIN_FLASH_CS, true);
	return read0 == 0x2c && read1 == 0x24;
}

void Fckafd::reset() {
	gpio_put(PIN_FLASH_CS, false);
	singleSpiTransfer(FLASH_CMD_RESET);
	gpio_put(PIN_FLASH_CS, true);
}

u8 Fckafd::getFeature(u8 featureRegister) {
	gpio_put(PIN_FLASH_CS, false);
	singleSpiTransfer(FLASH_CMD_GET_FEATURE);
	singleSpiTransfer(featureRegister);
	u8 ret = singleSpiTransfer();
	gpio_put(PIN_FLASH_CS, true);
	return ret;
}

void Fckafd::setFeature(u8 featureRegister, u8 data) {
	gpio_put(PIN_FLASH_CS, false);
	singleSpiTransfer(FLASH_CMD_SET_FEATURE);
	singleSpiTransfer(featureRegister);
	singleSpiTransfer(data);
	gpio_put(PIN_FLASH_CS, true);
}

void Fckafd::writeEnable() {
	gpio_put(PIN_FLASH_CS, false);
	singleSpiTransfer(FLASH_CMD_WRITE_ENABLE);
	gpio_put(PIN_FLASH_CS, true);
}
void Fckafd::writeDisable() {
	gpio_put(PIN_FLASH_CS, false);
	singleSpiTransfer(FLASH_CMD_WRITE_DISABLE);
	gpio_put(PIN_FLASH_CS, true);
}

bool Fckafd::checkFeature(u8 mask, u8 value, u8 featureRegister) {
	u8 read = getFeature();
	return (read & mask) == value;
}

void Fckafd::invalidateCaches() {
	cachedBlock = 0xFFFF;
	cachedPage = 0xFF;
	for (auto &sc : secCaches) {
		sc.block = 0xFFFF;
		sc.page = 0xFF;
		sc.sector = 0xFF;
		sc.prio = 0xFF;
	}
}

u16 Fckafd::getData(u16 block, u8 page, u16 start, u16 length, u8 *buf) {
	if ((u32)start + (u32)length > 2176) return 0;
	// TODO caching
	// if (start < 2048) {
	// 	u8 needSecs = 0b0000;
	// 	if (start < 512 && start + length > 0) needSecs |= 1u << 0;
	// 	if (start < 1024 && start + length > 512) needSecs |= 1u << 1;
	// 	if (start < 1536 && start + length > 1024) needSecs |= 1u << 2;
	// 	if (start < 2048 && start + length > 1536) needSecs |= 1u << 3;

	// 	u8 haveSecs = 0b0000;
	// 	for (auto &sc : secCaches) {
	// 		if (sc.prio != 0xFF && sc.block == block && sc.page == page) haveSecs |= 1u << sc.sector;
	// 	}

	// }
	pageRead(block, page);
	return readFromCache(block, start, length, buf);
}

void Fckafd::pageRead(u16 block, u8 page, bool getFeatureWait) {
	// if (cachedBlock == block && cachedPage == page) return;
	gpio_put(PIN_FLASH_CS, false);
	u32 addr = (page & 0x3F) | ((u32)block << 6);
	u8 buf[4] = {FLASH_CMD_PAGE_READ, (u8)(addr >> 16), (u8)(addr >> 8), (u8)addr};
	burstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
	cachedBlock = block;
	cachedPage = page;
	if (getFeatureWait) {
		while (checkFeature(0b1, 0b1)) {
			tight_loop_contents();
		}
	}
}

u16 Fckafd::readFromCache(u16 block, u16 start, u16 length, u8 *buf) {
	if ((u32)start + (u32)length > 2176) return 0;
	start |= (block & 0b1) << 12;
	const u16 lenBackup = length;
	gpio_put(PIN_FLASH_CS, false);
	u8 req[4] = {FLASH_CMD_READ_FROM_CACHE_X1, (u8)(start >> 8), (u8)start, 0};
	burstSpiWrite(4, req);
	burstSpiRead(length, buf);
	gpio_put(PIN_FLASH_CS, true);
	return lenBackup;
}

void Fckafd::eraseBlock(u16 block, bool getFeatureWait) {
	writeEnable();
	gpio_put(PIN_FLASH_CS, false);
	u32 addr = (u32) block << 6;
	u8 buf[4] = {FLASH_CMD_BLOCK_ERASE, (u8)(addr >> 16), (u8)(addr >> 8), (u8)(addr)};
	burstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
	if (getFeatureWait) {
		while (checkFeature(0b1, 0b1)) {
			tight_loop_contents();
		}
	}
}

u16 Fckafd::programLoad(u16 block, u16 start, u16 length, const u8 *buf) {
	if ((u32)start + (u32)length > 2176) return 0;
	writeEnable();
	start |= (block & 0b1) << 12;
	const u16 lenBackup = length;
	gpio_put(PIN_FLASH_CS, false);
	u8 req[3] = {FLASH_CMD_PROGRAM_LOAD_X1, (u8)(start >> 8), (u8)start};
	burstSpiWrite(3, req);
	burstSpiWrite(length, buf);
	gpio_put(PIN_FLASH_CS, true);
	cachedBlock = 0xFFFF;
	cachedPage = 0xFF;
	return lenBackup;
}

void Fckafd::programExecute(u16 block, u8 page, bool getFeatureWait) {
	writeEnable();
	gpio_put(PIN_FLASH_CS, false);
	u32 addr = (page & 0x3F) | ((u32)block << 6);
	u8 buf[4] = {FLASH_CMD_PROGRAM_EXECUTE, (u8)(addr >> 16), (u8)(addr >> 8), (u8)addr};
	burstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
	for (auto &sc : secCaches) {
		if (sc.block == block && sc.page == page) {
			sc.block = 0xFFFF;
			sc.page = 0xFF;
			sc.prio = 0xFF;
			sc.sector = 0xFF;
		}
	}
	if (getFeatureWait) {
		while (checkFeature(0b1, 0b1)) {
			tight_loop_contents();
		}
	}
}

//==============================FCKAFD====================================//
bool Fckafd::begin(pin_size_t ioBase, pin_size_t sckPin, pin_size_t csPin, bool &fsReady) {
	fsReady = false;

	// set up SPI
	gpio_init(csPin);
	gpio_set_dir(csPin, GPIO_OUT);
	gpio_put(csPin, true);
	gpio_set_slew_rate(csPin, GPIO_SLEW_RATE_FAST);
	gpio_set_slew_rate(sckPin, GPIO_SLEW_RATE_FAST);
	gpio_set_slew_rate(ioBase, GPIO_SLEW_RATE_FAST);
	blackboxSm = pio_claim_unused_sm(PIO_EXT_SPI_BB, true);
	Serial.printf("Got blackbox SM %d\n", blackboxSm);
	blackboxOffset = pio_add_program(PIO_EXT_SPI_BB, &extended_spi_program);
	pio_spi_init(PIO_EXT_SPI_BB, blackboxSm, blackboxOffset, 8, 1, sckPin, ioBase, ioBase + 1);

	// set up DMA channels for RX/TX
	dmaTxChannel = dma_claim_unused_channel(true);
	dmaRxChannel = dma_claim_unused_channel(true);
	dma_channel_config flashDmaTxConfig = dma_channel_get_default_config(dmaTxChannel);
	dma_channel_config flashDmaRxConfig = dma_channel_get_default_config(dmaRxChannel);
	channel_config_set_read_increment(&flashDmaTxConfig, true);
	channel_config_set_write_increment(&flashDmaTxConfig, false);
	channel_config_set_read_increment(&flashDmaRxConfig, false);
	channel_config_set_write_increment(&flashDmaRxConfig, true);
	channel_config_set_dreq(&flashDmaTxConfig, pio_get_dreq(PIO_EXT_SPI_BB, blackboxSm, true));
	channel_config_set_dreq(&flashDmaRxConfig, pio_get_dreq(PIO_EXT_SPI_BB, blackboxSm, false));
	channel_config_set_transfer_data_size(&flashDmaTxConfig, DMA_SIZE_8);
	channel_config_set_transfer_data_size(&flashDmaRxConfig, DMA_SIZE_8);
	dma_channel_set_config(dmaTxChannel, &flashDmaTxConfig, false);
	dma_channel_set_config(dmaRxChannel, &flashDmaRxConfig, false);
	dma_channel_set_write_addr(dmaTxChannel, &PIO_EXT_SPI_BB->txf[blackboxSm], false);
	dma_channel_set_read_addr(dmaRxChannel, &PIO_EXT_SPI_BB->rxf[blackboxSm], false);

	// find flash chip
	for (int i = 0; i < 50; i++) {
		if (checkReadId()) break;
		if (i == 49) return false;
		sleep_ms(2);
	}

	for (int i = 0; i < 50; i++) {
		if (checkReadId()) break;
		if (i == 49) return false;
		sleep_ms(2);
	}
	Serial.println("Found flash chip");

	// prepare flash chip
	setFeature(0xA0, 0x00); // disable block lock
	setFeature(0xB0, 0x50); // ECC enabled, access OTP/Parameter/UID
	setFeature(0xD0, 0x00); // default, just make sure we have selected die 0
	Serial.println("Disabled block lock, flash chip is functional");

	// read parameter page
	pageRead(0, 1, true);
	u8 *buf = (u8 *)malloc(2176);
	readFromCache(0, 0, 256, buf);
	strncpy(manufacturer, (char *)&buf[32], 13);
	manufacturer[12] = 0;
	strncpy(model, (char *)&buf[44], 21);
	model[20] = 0;
	manufacturerId = buf[64];
	pageSize = DECODE_U4(&buf[80]);
	spareSize = DECODE_U2(&buf[84]);
	pageCount = DECODE_U4(&buf[92]);
	blockCount = DECODE_U4(&buf[96]);
	maxBbBlock = blockCount - 40;
	maxProgTime = DECODE_U2(&buf[133]);
	maxEraseTime = DECODE_U2(&buf[135]);
	maxReadTime = DECODE_U2(&buf[137]);
	if (pageSize != 2048) {
		DEBUG_PRINTF("Page size refused %d\n", pageSize);
		free(buf);
		return false;
	}

	totalSize = pageSize * pageCount * blockCount;

	setFeature(0xB0, 0x10);
	invalidateCaches();

	// Accept Micron devices up to 4 Gb nominal size (512 MiB)
	if (manufacturerId != 0x2C || !totalSize || totalSize > 512 * 1024 * 1024) {
		DEBUG_PRINTF("Manufacturer ID %d, totalSize %d\n", manufacturerId, totalSize);
		free(buf);
		return false;
	}

	// Block 0:
	// Page 0 sector 0: Filesystem metadata: magic, version, anything else?
	// TODO Page 0 sector 1: ~Bad blocks that should be avoided (just set the corresponding bit to 0 to indicate that)
	// if filesystem magic not found: format filesystem (erase blocks 0-1999, recreate block 0 with no files in it; usually sector 1 of page 0 of block 0 would be cached and rewritten afterwards)

	pageRead(0, 0);
	readFromCache(0, 0, 256, buf);

	bool isValidFs = true;
	// Filesystem that Captures Kolibri's Awesome Flight Data
	if (memcmp(&buf[0], "FCKAFD", 6)) isValidFs = false;
	// check version 0.1.0
	if (buf[6] != 0) isValidFs = false;
	if (buf[7] != 1) isValidFs = false;
	if (buf[8] != 0) isValidFs = false;
	DEBUG_PRINTF("%s %d %d %d\n", buf, buf[6], buf[7], buf[8]);

	if (isValidFs) {
		u16 checkBlock = 0xFFFF;
		u16 firstFreeBlock = 0;

		for (int page = 1; page < 64; page++) {
			pageRead(0, page);
			for (int i = 0; i < 2; i++) {
				readFromCache(0, 1024 * i, 1024, buf);
				if (buf[0] != 0) break; // file table end reached
				if (buf[512] != 1) {
					// file incomplete, intiate check block
					checkBlock = DECODE_U2(&buf[3]);
					break;
				}
				firstFreeBlock = DECODE_U2(&buf[1]) + 1;
			}
			if (checkBlock != 0xFFFF) break;
		}
		if (checkBlock != 0xFFFF) {
			// TODO check block, broken file
		}
		fsReady = true;
		this->fsReady = true;
	}

	chipReady = true;

	free(buf);
	return true;
}

bool Fckafd::format(u8 partition) {
	if (!chipReady) return false;
	if (partition != 0) return false;
	for (int i = 0; i < maxBbBlock; i++) {
		eraseBlock(i);
		rp2040.wdt_reset();
	}
	u8 buf[9] = "FCKAFD";
	buf[6] = 0;
	buf[7] = 1;
	buf[8] = 0;
	DEBUG_PRINTF("fmt %s %d %d %d\n", buf, buf[6], buf[7], buf[8]);
	programLoad(0, 0, 9, buf);
	programExecute(0, 0);
	pageRead(0, 0);
	u8 buf2[9];
	readFromCache(0, 0, 9, buf2);
	fsReady = memcmp(buf, buf2, 9) == 0;
	return fsReady;
}

bool Fckafd::exists(u16 num) {
	if (!chipReady) return false;
	if (!fsReady) return false;
	u8 page = num / 2 + 1;
	u16 offset = (num % 2) * 1024;
	if (page >= 64) return false;
	u8 buf[3];
	pageRead(0, page);
	readFromCache(0, offset, 3, buf);
	if (buf[0] != 0 || DECODE_U2(&buf[1]) != num) return false;
	return true;
}

FlashFile Fckafd::open(u16 num, oflag_t oflag) {
	printfIndMessage("Open %d", num);
	if (!fsReady) return FlashFile();
	printIndMessage("FS ready");
	if (oflag == O_RDONLY) {
		return FlashFile(0, num, false, *this);
	}
	if (oflag == O_WRITE | O_CREAT) {
		DEBUG_PRINTF("Will create file %d\n", num);
		return FlashFile(0, num, true, *this);
	}
	printIndMessage("Flag???");
	return FlashFile();
}

u16 Fckafd::getNewBbFileNum() {
	DEBUG_PRINTLN("getNewBbFileNum");
	if (!chipReady) return 0xFFFF;
	DEBUG_PRINTLN("chip ready");
	if (!fsReady) return 0xFFFF;
	DEBUG_PRINTLN("FCKAFD ready");
	u8 buf[3];
	i32 highest = -1;
	for (int page = 1; page < 64; page++) {
		pageRead(0, page);
		for (int offset = 0; offset < 2048; offset += 1024) {
			readFromCache(0, offset, 3, buf);
			if (buf[0] != 0x00) continue;
			i32 num = DECODE_U2(&buf[1]);
			if (num > highest) highest = num;
		}
	}
	DEBUG_PRINTF("Suggested number %d\n", highest + 1);
	return highest + 1;
}

//==============================FlashFile================================//

Fckafd dummy;
FlashFile::FlashFile() : fck(&dummy) {
	isOpen = false;
}

FlashFile::FlashFile(u8 partition, u16 fileNum, bool forWrite, Fckafd &fs) : fck(&fs) {
	this->fileNum = fileNum;
	this->writeAccess = forWrite;
	u8 buf[130];

	if (forWrite) {
		u16 freePage = 0xFFFF;
		u16 freeOffset = 0xFFFF;
		i32 maxBlock = 0;
		for (int page = 1; page < 64; page++) {
			for (int offset = 0; offset < 2048; offset += 1024) {
				fck->getData(0, page, offset, 9, buf);

				// remember page and offset so we can place the file here later
				if (buf[0] == 0xFF && freePage == 0xFFFF) {
					freePage = page;
					freeOffset = offset;
				}

				// if we find a file, check the fileNum and mark the highest used block
				if (buf[0] == 0x00) {
					u16 fn = DECODE_U2(&buf[1]);
					if (fn == fileNum) {
						// found file, do not open
						isOpen = false;
						DEBUG_PRINTLN("Found File. Do not open");
						return;
					}

					firstBlock = DECODE_U2(&buf[3]);
					fck->getData(0, page, offset + 512, 7, buf);
					if (buf[0] != 1) {
						// failed finishing file (should not happen when the file is being cleaned on mount)
						isOpen = false;
						DEBUG_PRINTLN("Found incomplete file. Cannot create a new one");
						return;
					}

					u16 endBlock = DECODE_U2(&buf[1]);
					if (endBlock > maxBlock) maxBlock = endBlock;
				}
			}
		}

		if (freePage == 0xFFFF) {
			isOpen = false;
			DEBUG_PRINTLN("Could not find a free meta page to create the file in");
			return;
		}
		DEBUG_PRINTF("Found a free page %d %d \n", freePage, freeOffset);

		firstBlock = maxBlock + 1;
		currentBlock = firstBlock;
		lastBlock = firstBlock;
		buf[0] = 0;
		buf[1] = fileNum;
		buf[2] = fileNum >> 8;
		buf[3] = firstBlock;
		buf[4] = firstBlock >> 8;
		startTime = rtcGetUnixTimestamp();
		memcpy(&buf[5], &startTime, 4);
		fck->programLoad(0, freeOffset, 9, buf);
		fck->programExecute(0, freePage);
		metaPage = freePage;
		metaPagePart = freeOffset / 1024;

		DEBUG_PRINTF("Created file %d for write with its first block %d, on meta page %d at offset %d. Start time %d\n", fileNum, firstBlock, freePage, freeOffset, startTime);
	} else {
		bool foundFile = false;
		// for reading
		for (int page = 1; page < 64; page++) {
			for (int offset = 0; offset < 2048; offset += 1024) {
				fck->getData(0, page, offset, 9, buf);

				if (buf[0] == 0x00) {
					// found a file, check if it is this
					u16 fn = DECODE_U2(&buf[1]);
					if (fn == fileNum) {
						firstBlock = DECODE_U2(&buf[3]);
						currentBlock = firstBlock;
						startTime = DECODE_U4(&buf[5]);
						fck->getData(0, page, offset + 512, 7, buf);
						if (buf[0] != 1) continue; // failed finishing file (should not happen when the file is being cleaned on mount)
						lastBlock = DECODE_U2(&buf[1]);
						fileSize = DECODE_U4(&buf[3]);
						foundFile = true;
						fck->getData(0, page, offset + 512 + 126, 130, buf);
						printfIndMessage("Opening file %d for read on page %d with offset %d. First block %d, last block %d, total file size %d. Start time %d", fileNum, page, offset, firstBlock, lastBlock, fileSize, startTime);
						for (int i = 0; i < 26; i++) {
							int pos = i * 5;
							u32 bytePos = DECODE_U4(&buf[pos]);
							u8 byte = buf[pos + 4];
							if (bytePos == 0xFFFFFFFF) break;
							corrBytes[i].pos = bytePos;
							corrBytes[i].byte = byte;
							corrCount++;
						}
						break;
					}
				}
			}
			if (foundFile) break;
		}
		if (!foundFile) isOpen = false;
	}
}

bool FlashFile::seek(u32 newPos) {
	if (!isOpen || newPos > fileSize) return false;
	if (writeAccess && !correctionMode) {
		privateFlush();
	}

	currentBlock = firstBlock + newPos / (fck->pageCount * fck->pageSize);
	currentPage = (newPos % (fck->pageCount * fck->pageSize)) / fck->pageSize;
	currentPagePos = newPos % fck->pageSize;
	currentFilePos = newPos;
	return true;
}

void FlashFile::moveCursorFwd(u32 count) {
	currentFilePos += count;
	currentPagePos += count;
	while (currentPagePos >= fck->pageSize) {
		currentPagePos -= fck->pageSize;
		currentPage++;
		if (currentPage >= fck->pageCount) {
			currentPage -= fck->pageCount;
			currentBlock++;
		}
	}
}

// Stream
int FlashFile::available() {
	if (!isOpen || writeAccess) return 0;
	i32 av = (i32)fileSize - (i32)currentFilePos;
	if (av < 0) return 0;
	return av;
}
int FlashFile::read() {
	int p = peek();
	if (p == -1) return -1;
	moveCursorFwd();
	return p;
}
i32 FlashFile::read(u8 *buffer, size_t length) {
	if (!isOpen || writeAccess) return -1;
	if (!length) return 0;
	if (!available()) return 0;

	const u32 endExcl = currentFilePos + length;
	if (endExcl > fileSize) length = fileSize - currentFilePos;

	const u32 startFPos = currentFilePos;

	// simplest read: no page boundary
	if (currentPagePos + length <= 2048) {
		fck->getData(currentBlock, currentPage, currentPagePos, length, buffer);
		for (auto &cb : corrBytes) {
			if (cb.pos >= startFPos && cb.pos < endExcl) {
				buffer[cb.pos - startFPos] = cb.byte;
			}
		}
		moveCursorFwd(length);
		return length;
	}

	// else read first (part) page
	u16 thisLength = 2048 - currentPagePos;
	size_t bufPos = 0;
	fck->getData(currentBlock, currentPage, currentPagePos, thisLength, buffer);
	bufPos += thisLength;
	moveCursorFwd(thisLength);

	// as long as we can still read full pages
	thisLength = fck->pageSize;
	while (bufPos + thisLength < length) {
		fck->getData(currentBlock, currentPage, 0, thisLength, buffer + bufPos);
		bufPos += thisLength;
		moveCursorFwd(thisLength);
	}

	// read remaining bytes
	thisLength = length - bufPos;
	fck->getData(currentBlock, currentPage, 0, thisLength, buffer + bufPos);
	bufPos += thisLength;
	moveCursorFwd(thisLength);

	for (auto &cb : corrBytes) {
		if (cb.pos >= startFPos && cb.pos < endExcl) {
			buffer[cb.pos - startFPos] = cb.byte;
		}
	}
	return length;
}
int FlashFile::peek() {
	if (!isOpen || writeAccess || !available()) return -1;
	for (auto &cb : corrBytes) {
		if (cb.pos == currentFilePos) {
			return cb.byte;
		}
	}
	u8 buf;
	fck->getData(currentBlock, currentPage, currentPagePos, 1, &buf);
	return buf;
}

// Print
size_t FlashFile::write(const uint8_t *buffer, size_t size) {
	if (!writeAccess || !isOpen) return 0;

	if (correctionMode) {
		size_t written = 0;
		for (size_t i = 0; i < size; i++) {
			written += write(buffer[i]);
		}
		return written;
	}

	u16 sectorPos = currentPagePos % 512;

	// partial sector write (don't send to flash)
	u16 maxWrite = 512 - sectorPos;
	if (maxWrite > size) {
		memcpy(writeBuf + sectorPos, buffer, size);
		moveCursorFwd(size);
		fileSize += size;
		return size;
	}

	// finish the current sector and send it off
	size_t wrPos = 0;
	size_t wrLeft = size;
	memcpy(writeBuf + sectorPos, buffer, maxWrite);
	fck->programLoad(currentBlock, currentPagePos - sectorPos, 512, writeBuf);
	fck->programExecute(currentBlock, currentPage);
	moveCursorFwd(maxWrite);
	wrPos += maxWrite;
	wrLeft -= maxWrite;
	if (currentBlock > fck->maxBbBlock) {
		correctionMode = true;
		fileSize += wrPos;
		return wrPos;
	}

	// any other sectors able to be fully written?
	while (wrLeft >= 512) {
		memcpy(writeBuf, buffer + wrPos, 512);
		fck->programLoad(currentBlock, currentPagePos, 512, writeBuf);
		fck->programExecute(currentBlock, currentPage);
		moveCursorFwd(512);
		wrPos += 512;
		wrLeft -= 512;
		if (currentBlock > fck->maxBbBlock) {
			correctionMode = true;
			fileSize += wrPos;
			return wrPos;
		}
	}
	fileSize += size;

	// no data left
	if (!wrLeft) return size;

	// partial sector left (only write to writeBuf)
	memcpy(writeBuf, buffer + wrPos, wrLeft);
	moveCursorFwd(wrLeft);
	return size;

	// TODO maximum file size
}
size_t FlashFile::write(uint8_t data) {
	if (!writeAccess || !isOpen) return 0;

	if (correctionMode) {
		if (corrCount >= 26) return 0;
		corrBytes[corrCount].pos = currentFilePos;
		moveCursorFwd();
		corrBytes[corrCount++].byte = data;
		return 1;
	}

	u16 sectorPos = currentPagePos % 512;
	writeBuf[sectorPos] = data;
	if (sectorPos == 511) {
		fck->programLoad(currentBlock, currentPagePos - 511, 512, writeBuf);
		fck->programExecute(currentBlock, currentPage);
	}
	moveCursorFwd();
	if (currentBlock > fck->maxBbBlock) {
		maxBlock = currentBlock;
		correctionMode = true;
	}
	fileSize++;
	return 1;
}
int FlashFile::availableForWrite() {
	if (!writeAccess || !isOpen) return 0;
	// TODO bytes that can be written immediately
	return 1;
}
void FlashFile::flush() {
	// no flushing supported due to unpredictability. privateFlush() exists for internal flushing
	return;
}

void FlashFile::close() {
	if (!isOpen || !writeAccess) return;
	if (!correctionMode) privateFlush();

	u8 buf[256];
	memset(buf, 0xFF, 256);
	buf[0] = 1;
	buf[1] = maxBlock;
	buf[2] = maxBlock >> 8;
	memcpy(&buf[3], &fileSize, 4);
	for (int i = 0; i < 26; i++) {
		int pos = i * 5 + 126;
		memcpy(&buf[pos], &corrBytes[i].pos, 4);
		buf[pos + 4] = corrBytes[i].byte;
	}

	fck->programLoad(0, metaPagePart * 1024 + 512, 256, buf);
	fck->programExecute(0, metaPage);
	DEBUG_PRINTF("Closing file %d on max block %d with offset %d on metaPage %d\n", fileNum, maxBlock, metaPagePart * 1024, metaPage);

	isOpen = false;
}

void FlashFile::privateFlush() {
	if (!writeAccess || !isOpen) return;
	if (currentPagePos == 0) return;
	fck->programLoad(currentBlock, currentPagePos / 512 * 512, currentPagePos % 512, writeBuf);
	fck->programExecute(currentBlock, currentPage);
	maxBlock = currentBlock;

	correctionMode = true;
	DEBUG_PRINTLN("Flushed file, going to correction mode now");
}

#endif
