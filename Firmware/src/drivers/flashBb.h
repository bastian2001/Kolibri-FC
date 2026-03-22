#pragma once
#if BLACKBOX_STORAGE == FLASH_BB

class Fckafd;

typedef struct correctionByte {
	u32 pos = 0xFFFFFFFF;
	u8 byte = 0;
} CorrectionByte;

class FlashFile : public Stream {
public:
	FlashFile();
	FlashFile(u8 partition, u16 fileNum, bool forWrite, Fckafd &fs);

	FlashFile(const FlashFile &) = delete; // no copies
	FlashFile &operator=(const FlashFile &) = delete; // no copies

	FlashFile(FlashFile &&) = default; // allow moves
	FlashFile &operator=(FlashFile &&) = default; // allow moves

	bool seek(u32 pos);
	size_t position() { return currentFilePos; };

	// Stream
	int available() override;
	int read() override;
	i32 read(u8 *buffer, size_t length);
	int peek() override;

	// Print
	size_t write(const uint8_t *buffer, size_t size) override;
	size_t write(uint8_t) override;
	int availableForWrite() override;
	void flush() override;

	void close();

	operator bool() { return isOpen; };

	size_t size() { return fileSize; };

private:
	void moveCursorFwd(u32 count = 1);
	void privateFlush();

	bool isOpen = true;
	bool writeAccess = false;
	CorrectionByte corrBytes[26];
	bool correctionMode = false;
	u8 corrCount = 0;
	u32 fileSize = 0;
	u16 firstBlock = 0;
	u16 lastBlock = 0;
	u32 startTime = 0;
	u16 fileNum = 0;
	char fileName[33];

	Fckafd *fck;

	u16 currentBlock = 0;
	u8 currentPage = 0;
	u8 writeBuf[512];
	u32 currentFilePos = 0;
	u16 currentPagePos = 0;

	u8 metaPage = 0xFF;
	u8 metaPagePart = 0;
	u16 maxBlock = 0;
};

#define CACHED_SECTORS 3

typedef struct sectorCache {
	u8 buf[512];
	u16 block = 0xFFFF;
	u8 page = 0xFF;
	u8 sector = 0xFF;
	u8 prio = 0xFF;
} SectorCache;

// Filesystem that Captures Kolibri's Awesome Flight Data
class Fckafd {
public:
	Fckafd() = default;
	Fckafd(const Fckafd &) = delete;
	Fckafd &operator=(const Fckafd &) = delete;

	/**
	 * @brief Initialize flash chip, check for file system
	 *
	 * @param ioBase pin of IO0. IO1 must be the one immediately following
	 * @param sckPin SPI Clock pin
	 * @param csPin Chip Select pin
	 * @param fsReady true is written here if the filesystem was found, false otherwise
	 * @return true if flash chip found and compatible
	 * @return false if not found or not compatible
	 */
	bool begin(pin_size_t ioBase, pin_size_t sckPin, pin_size_t csPin, bool &fsReady);

	/**
	 * @brief formats a partition on the flash chip
	 *
	 * @param partition 0 for blackbox partition, 1 for audio partition
	 * @return true if success
	 * @return false if failed
	 */
	bool format(u8 partition);

	/** Test for the existence of a file
	 *
	 * @param path Path of the file to be tested for. Must lie within /audio on second partition
	 *
	 * @return true if the file exists else false.
	 */
	bool exists(const char *path) { return false; };

	/**
	 * @brief Test for the existence of a file
	 *
	 * @param num blackbox file num
	 * @return true if this file exists
	 * @return false if it doesn't exist
	 */
	bool exists(u16 num);

	/**
	 * @brief Opens a file
	 *
	 * @param num blackbox file num (first partition)
	 * @param oflag Either O_RDONLY or O_WRITE | O_CREAT. No read and write combined. You can replace a max of 26 bytes in a file after you've written them. Once an O_WRITE | O_CREAT file is closed, you cannot edit it anymore.
	 * @return FlashBbFile The opened file, nullptr if file already exists and trying to write
	 */
	FlashFile open(u16 num, oflag_t oflag = O_RDONLY);

	/**
	 * @brief Opens a file
	 *
	 * @param path path to file. Must lie within /audio on second partition
	 * @param oflag Either O_RDONLY or O_WRITE | O_CREAT. No read and write combined. Once an O_WRITE | O_CREAT file is closed, you cannot edit it anymore. No modification of any written byte. No deletion.
	 * @return FlashBbFile The opened file, nullptr if file already exists and trying to write
	 */
	FlashFile open(const char *path, oflag_t oflag = O_RDONLY) { return FlashFile(); };

	/// remove not supported
	bool remove(const char *path) { return false; };
	/// remove not supported
	bool remove(const u16 fileNum) { return false; };

	/**
	 * @brief Get the file num that is one higher than the highest file num currently present
	 *
	 * @return u16 file num or 0xFFFF if blackbox full
	 */
	u16 getNewBbFileNum();

	char manufacturer[13];
	char model[21];
	u32 totalSize = 0;

	u16 cachedBlock = 0xFFFF;
	u8 cachedPage = 0xFF;

	u16 programLoad(u16 block, u16 start, u16 length, const u8 *buf);
	void programExecute(u16 block, u8 page, bool getFeatureWait = true);
	u16 getData(u16 block, u8 page, u16 start, u16 length, u8 *buf);
	void invalidateCaches();
	void eraseBlock(u16 block, bool getFeatureWait = true);
	u8 getFeature(u8 featureRegister = 0xC0);

	u32 pageSize = 0;
	u32 pageCount = 0;
	u32 blockCount = 0;
	u32 maxBbBlock = 0;

private:
	u8 singleSpiTransfer(u8 txByte = 0);
	void burstSpiRead(u16 len, void *dst);
	void burstSpiWrite(u16 len, const void *src);
	bool checkReadId();
	void reset();
	void setFeature(u8 featureRegister, u8 data);
	void writeEnable();
	void writeDisable();
	u16 readFromCache(u16 block, u16 start, u16 length, u8 *buf);
	void pageRead(u16 block, u8 page, bool getFeatureWait = true);
	bool checkFeature(u8 mask, u8 value, u8 featureRegister = 0xC0);

	u8 blackboxSm;
	u8 blackboxOffset;

	SectorCache secCaches[CACHED_SECTORS];

	u8 dmaTxChannel, dmaRxChannel;
	bool chipReady = false;
	bool fsReady = false;
	u8 manufacturerId = 0xFF;
	u16 spareSize = 0;
	u32 maxProgTime = 0;
	u32 maxEraseTime = 0;
	u32 maxReadTime = 0;

	u32 firstFreeBlock = 0;
};

#endif
