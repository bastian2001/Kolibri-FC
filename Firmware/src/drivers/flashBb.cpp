#include "global.h"
#include "pioasm/extended_spi.pio.h"

static u8 blackboxSm;
static u8 blackboxOffset;

static u8 flashDmaTxChannel, flashDmaRxChannel;

static bool flashReady = false;
static char flashManufacturer[13];
static char flashModel[21];
static u8 flashManufacturerId = 0xFF;
static u32 flashPageSize = 0;
static u16 flashSpareSize = 0;
static u32 flashPageCount = 0;
static u32 flashBlockCount = 0;
static u32 flashMaxProgTime = 0;
static u32 flashMaxEraseTime = 0;
static u32 flashMaxReadTime = 0;
static u32 flashTotalSize = 0;

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
u8 flashSingleSpiTransfer(u8 txByte = 0) {
	pio_sm_clear_fifos(PIO_EXT_SPI_BB, blackboxSm);
	pio_sm_put_blocking(PIO_EXT_SPI_BB, blackboxSm, ((u32)txByte) << 24);
	return pio_sm_get_blocking(PIO_EXT_SPI_BB, blackboxSm);
}
void flashBurstSpiRead(u16 len, u8 *dst) {
	if (dst == nullptr) return;
	u8 dummy[len];
	pio_sm_clear_fifos(PIO_EXT_SPI_BB, blackboxSm);
	dma_channel_abort(flashDmaTxChannel);
	dma_channel_abort(flashDmaRxChannel);
	while (dma_channel_is_busy(flashDmaTxChannel) || dma_channel_is_busy(flashDmaRxChannel) || dma_hw->abort & (1u << flashDmaTxChannel | 1u << flashDmaRxChannel)) {
		// wait until busy is deasserted and abort is cleared (12.6.8.3 and E5)
		tight_loop_contents();
	}

	dma_channel_set_write_addr(flashDmaRxChannel, dst, false);
	dma_channel_set_transfer_count(flashDmaRxChannel, len, false);
	dma_channel_set_read_addr(flashDmaTxChannel, dummy, false);
	dma_channel_set_transfer_count(flashDmaTxChannel, len, false);
	dma_start_channel_mask((1u << flashDmaTxChannel) | (1u << flashDmaRxChannel));

	while (dma_channel_is_busy(flashDmaTxChannel) || dma_channel_is_busy(flashDmaRxChannel)) {
		tight_loop_contents();
	}
}
void flashBurstSpiWrite(u16 len, const u8 *src) {
	if (src == nullptr) return;
	u8 dummy[len];
	pio_sm_clear_fifos(PIO_EXT_SPI_BB, blackboxSm);
	dma_channel_abort(flashDmaTxChannel);
	dma_channel_abort(flashDmaRxChannel);
	while (dma_channel_is_busy(flashDmaTxChannel) || dma_channel_is_busy(flashDmaRxChannel) || dma_hw->abort & (1u << flashDmaTxChannel | 1u << flashDmaRxChannel)) {
		// wait until busy is deasserted and abort is cleared (12.6.8.3 and E5)
		tight_loop_contents();
	}

	dma_channel_set_transfer_count(flashDmaRxChannel, len, false);
	dma_channel_set_write_addr(flashDmaRxChannel, dummy, false);
	dma_channel_set_transfer_count(flashDmaTxChannel, len, false);
	dma_channel_set_read_addr(flashDmaTxChannel, src, false);
	dma_start_channel_mask((1u << flashDmaTxChannel) | (1u << flashDmaRxChannel));

	while (dma_channel_is_busy(flashDmaRxChannel) || dma_channel_is_busy(flashDmaTxChannel)) {
		tight_loop_contents();
	}
}

bool flashCheckReadId() {
	gpio_put(PIN_FLASH_CS, false);
	flashSingleSpiTransfer(FLASH_CMD_READ_ID);
	flashSingleSpiTransfer(); // dummy byte
	u8 read0 = flashSingleSpiTransfer();
	u8 read1 = flashSingleSpiTransfer();
	gpio_put(PIN_FLASH_CS, true);
	return read0 == 0x2c && read1 == 0x24;
}

void flashReset() {
	gpio_put(PIN_FLASH_CS, false);
	flashSingleSpiTransfer(FLASH_CMD_RESET);
	gpio_put(PIN_FLASH_CS, true);
}

u8 flashGetFeature(u8 featureRegister = 0xC0) {
	gpio_put(PIN_FLASH_CS, false);
	flashSingleSpiTransfer(FLASH_CMD_GET_FEATURE);
	flashSingleSpiTransfer(featureRegister);
	u8 ret = flashSingleSpiTransfer();
	gpio_put(PIN_FLASH_CS, true);
	return ret;
}

void flashSetFeature(u8 featureRegister, u8 data) {
	gpio_put(PIN_FLASH_CS, false);
	flashSingleSpiTransfer(FLASH_CMD_SET_FEATURE);
	flashSingleSpiTransfer(featureRegister);
	flashSingleSpiTransfer(data);
	gpio_put(PIN_FLASH_CS, true);
}

void flashWriteEnable() {
	gpio_put(PIN_FLASH_CS, false);
	flashSingleSpiTransfer(FLASH_CMD_WRITE_ENABLE);
	gpio_put(PIN_FLASH_CS, true);
}
void flashWriteDisable() {
	gpio_put(PIN_FLASH_CS, false);
	flashSingleSpiTransfer(FLASH_CMD_WRITE_DISABLE);
	gpio_put(PIN_FLASH_CS, true);
}

bool flashCheckFeature(u8 mask, u8 value, u8 featureRegister = 0xC0) {
	u8 read = flashGetFeature();
	return (read & mask) == value;
}

void flashPageRead(u16 block, u8 page, bool getFeatureWait = true) {
	gpio_put(PIN_FLASH_CS, false);
	u32 addr = (page & 0x3F) | ((u32)block << 6);
	u8 buf[4] = {FLASH_CMD_PAGE_READ, (u8)(addr >> 16), (u8)(addr >> 8), (u8)addr};
	flashBurstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
	if (getFeatureWait) {
		while (flashCheckFeature(0b1, 0b1)) {
			tight_loop_contents();
		}
	}
}

u16 flashReadFromCache(u16 block, u16 start, u16 length, u8 *buf) {
	if ((u32)start + (u32)length > 2176) return 0;
	start |= (block & 0b1) << 12;
	const u16 lenBackup = length;
	gpio_put(PIN_FLASH_CS, false);
	u8 req[4] = {FLASH_CMD_READ_FROM_CACHE_X1, (u8)(start >> 8), (u8)start, 0};
	flashBurstSpiWrite(4, req);
	flashBurstSpiRead(length, buf);
	gpio_put(PIN_FLASH_CS, true);
	return lenBackup;
}

void flashErase(u16 block, bool getFeatureWait = true) {
	gpio_put(PIN_FLASH_CS, false);
	u8 buf[4] = {FLASH_CMD_BLOCK_ERASE, 0, (u8)(block >> 8), (u8)block};
	flashBurstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
	if (getFeatureWait) {
		while (flashCheckFeature(0b1, 0b1)) {
			tight_loop_contents();
		}
	}
}

u16 flashProgramLoad(u16 block, u16 start, u16 length, const u8 *buf) {
	if ((u32)start + (u32)length > 2176) return 0;
	start |= (block & 0b1) << 12;
	const u16 lenBackup = length;
	gpio_put(PIN_FLASH_CS, false);
	u8 req[3] = {FLASH_CMD_PROGRAM_LOAD_X1, (u8)(start >> 8), (u8)start};
	flashBurstSpiWrite(3, req);
	flashBurstSpiWrite(length, buf);
	gpio_put(PIN_FLASH_CS, true);
	return lenBackup;
}

void flashProgramExecute(u16 block, u8 page, bool getFeatureWait = true) {
	gpio_put(PIN_FLASH_CS, false);
	u32 addr = (page & 0x3F) | ((u32)block << 6);
	u8 buf[4] = {FLASH_CMD_PROGRAM_EXECUTE, (u8)(addr >> 16), (u8)(addr >> 8), (u8)addr};
	flashBurstSpiWrite(4, buf);
	gpio_put(PIN_FLASH_CS, true);
	if (getFeatureWait) {
		while (flashCheckFeature(0b1, 0b1)) {
			tight_loop_contents();
		}
	}
}

void initFlashBb() {
	sleep_ms(3000);

	// set up SPI
	gpio_init(PIN_FLASH_CS);
	gpio_set_dir(PIN_FLASH_CS, GPIO_OUT);
	gpio_put(PIN_FLASH_CS, true);
	gpio_set_slew_rate(PIN_FLASH_CS, GPIO_SLEW_RATE_FAST);
	gpio_set_slew_rate(PIN_FLASH_SCLK, GPIO_SLEW_RATE_FAST);
	gpio_set_slew_rate(PIN_FLASH_IO_BASE, GPIO_SLEW_RATE_FAST);
	blackboxSm = pio_claim_unused_sm(PIO_EXT_SPI_BB, true);
	Serial.printf("Got blackbox SM %d\n", blackboxSm);
	blackboxOffset = pio_add_program(PIO_EXT_SPI_BB, &extended_spi_program);
	pio_spi_init(PIO_EXT_SPI_BB, blackboxSm, blackboxOffset, 8, 1, PIN_FLASH_SCLK, PIN_FLASH_IO_BASE, PIN_FLASH_IO_BASE + 1);

	// set up DMA channels for RX/TX
	flashDmaTxChannel = dma_claim_unused_channel(true);
	flashDmaRxChannel = dma_claim_unused_channel(true);
	dma_channel_config flashDmaTxConfig = dma_channel_get_default_config(flashDmaTxChannel);
	dma_channel_config flashDmaRxConfig = dma_channel_get_default_config(flashDmaRxChannel);
	channel_config_set_read_increment(&flashDmaTxConfig, true);
	channel_config_set_write_increment(&flashDmaTxConfig, false);
	channel_config_set_read_increment(&flashDmaRxConfig, false);
	channel_config_set_write_increment(&flashDmaRxConfig, true);
	channel_config_set_dreq(&flashDmaTxConfig, pio_get_dreq(PIO_EXT_SPI_BB, blackboxSm, true));
	channel_config_set_dreq(&flashDmaRxConfig, pio_get_dreq(PIO_EXT_SPI_BB, blackboxSm, false));
	channel_config_set_transfer_data_size(&flashDmaTxConfig, DMA_SIZE_8);
	channel_config_set_transfer_data_size(&flashDmaRxConfig, DMA_SIZE_8);
	dma_channel_set_config(flashDmaTxChannel, &flashDmaTxConfig, false);
	dma_channel_set_config(flashDmaRxChannel, &flashDmaRxConfig, false);
	dma_channel_set_write_addr(flashDmaTxChannel, &PIO_EXT_SPI_BB->txf[blackboxSm], false);
	dma_channel_set_read_addr(flashDmaRxChannel, &PIO_EXT_SPI_BB->rxf[blackboxSm], false);

	// find flash chip
	for (int i = 0; i < 50; i++) {
		if (flashCheckReadId()) break;
		if (i == 49) return;
		sleep_ms(2);
	}

	for (int i = 0; i < 50; i++) {
		if (flashCheckReadId()) break;
		if (i == 49) return;
		sleep_ms(2);
	}
	Serial.println("Found flash chip");

	// prepare flash chip
	flashSetFeature(0xA0, 0x00); // disable block lock
	flashSetFeature(0xB0, 0x50); // ECC enabled, access OTP/Parameter/UID
	flashSetFeature(0xD0, 0x00); // default, just make sure we have selected die 0
	Serial.println("Disabled block lock, flash chip is functional");

	// read parameter page
	flashPageRead(0, 1, true);
	u8 buf[256];
	flashReadFromCache(0, 0, 256, buf);
	strncpy(flashManufacturer, (char *)&buf[32], 13);
	flashManufacturer[12] = 0;
	strncpy(flashModel, (char *)&buf[44], 21);
	flashModel[20] = 0;
	flashManufacturerId = buf[64];
	flashPageSize = DECODE_U4(&buf[80]);
	flashSpareSize = DECODE_U2(&buf[84]);
	flashPageCount = DECODE_U4(&buf[92]);
	flashBlockCount = DECODE_U4(&buf[96]);
	flashMaxProgTime = DECODE_U2(&buf[133]);
	flashMaxEraseTime = DECODE_U2(&buf[135]);
	flashMaxReadTime = DECODE_U2(&buf[137]);

	flashTotalSize = flashPageSize * flashPageCount * flashBlockCount;

	flashSetFeature(0xB0, 0x10);

	// Accept Micron devices up to 4 Gb nominal size (512 MiB)
	if (flashManufacturerId != 0x2C || !flashTotalSize || flashTotalSize > 512 * 1024 * 1024) {
		return;
	}

	flashReady = true;
}
