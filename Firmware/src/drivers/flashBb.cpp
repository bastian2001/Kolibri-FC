
#if BLACKBOX_STORAGE == FLASH_BB

#include "global.h"
#include "pioasm/extended_spi.pio.h"

Fckafd fck;

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
	u8 dummy[len];
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
}
void Fckafd::burstSpiWrite(u16 len, const u8 *src) {
	if (src == nullptr) return;
	u8 dummy[len];
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

void Fckafd::pageRead(u16 block, u8 page, bool getFeatureWait) {
	gpio_put(PIN_FLASH_CS, false);
	u32 addr = (page & 0x3F) | ((u32)block << 6);
	u8 buf[4] = {FLASH_CMD_PAGE_READ, (u8)(addr >> 16), (u8)(addr >> 8), (u8)addr};
	burstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
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
	u8 buf[4] = {FLASH_CMD_BLOCK_ERASE, 0, (u8)(block >> 8), (u8)block};
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
	return lenBackup;
}

void Fckafd::programExecute(u16 block, u8 page, bool getFeatureWait) {
	writeEnable();
	gpio_put(PIN_FLASH_CS, false);
	u32 addr = (page & 0x3F) | ((u32)block << 6);
	u8 buf[4] = {FLASH_CMD_PROGRAM_EXECUTE, (u8)(addr >> 16), (u8)(addr >> 8), (u8)addr};
	burstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
	if (getFeatureWait) {
		while (checkFeature(0b1, 0b1)) {
			tight_loop_contents();
		}
	}
}

void printFirstBytes() {
	fck.pageRead(0, 0);
	u8 buf[256];
	fck.readFromCache(0, 0, 256, buf);
	for (int i = 0; i < 256; i++) {
		if (i % 16 == 0) Serial.printf("\n%03d 0x%02X: ", i, i);
		if (i % 8 == 0) Serial.print(' ');
		Serial.printf("%02X", buf[i]);
	}
	Serial.println();
}

void initFlashBb() {
	sleep_ms(3000);
	bool fsReady;
	Serial.println(fck.begin(PIN_FLASH_IO_BASE, PIN_FLASH_SCLK, PIN_FLASH_CS, fsReady) ? "Found flash" : "Flash error");
	printFirstBytes();
	Serial.println(fsReady ? "FS ready" : "FS not ready");
	if (fsReady) {
		Serial.println("Nuking FS");
		fck.eraseBlock(0);
		sleep_ms(2000);
	}
	printFirstBytes();

	Serial.println("(Re)creating FS");
	Serial.flush();
	fck.format(0);
	printFirstBytes();

	Serial.println(fck.getNewBbFileNum());
	Serial.flush();

	for (;;)
		;
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
	u8 buf[2176];
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

	totalSize = pageSize * pageCount * blockCount;

	setFeature(0xB0, 0x10);

	// Accept Micron devices up to 4 Gb nominal size (512 MiB)
	if (manufacturerId != 0x2C || !totalSize || totalSize > 512 * 1024 * 1024) {
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
	}

	chipReady = true;

	return true;
}

bool Fckafd::format(u8 partition) {
	if (!chipReady) return false;
	if (partition != 0) return false;
	for (int i = 0; i < maxBbBlock; i++) {
		eraseBlock(i);
	}
	u8 buf[9] = "FCKAFD";
	buf[6] = 0;
	buf[7] = 1;
	buf[8] = 0;
	Serial.printf("fmt %s %d %d %d\n", buf, buf[6], buf[7], buf[8]);
	programLoad(0, 0, 9, buf);
	programExecute(0, 0);
	fsReady = true;
	return true;
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

u16 Fckafd::getNewBbFileNum() {
	if (!chipReady) return 0xFFFF;
	if (!fsReady) return 0xFFFF;
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
	return highest + 1;
}

#endif
