#pragma once
#include "global.h"

class BufferedWriter {
public:
	BufferedWriter(u32 size = 256, Stream *serial, u32 baud = 115200) {
		this->buffer = new char[size];
		this->serial = serial;
		this->baud = baud;
	}
	~BufferedWriter();

	int availableForWrite();

	// Add public methods here

private:
	char *buffer;
	Stream *serial;
	u32 baud;
	// Add private members here
};
