#pragma once

#include "global.h"

#define MSP_DP_SPEED 115200

namespace MspDpFn {
	enum : u8 {
		HEARTBEAT = 0,
		RELEASE = 1,
		CLEAR_SCREEN = 2,
		WRITE_STRING = 3,
		DRAW_SCREEN = 4,
		OPTIONS = 5,
		SYS = 6
	};
}

void mspDisplayportLoop();
void initMspDisplayport();
void onSetCanvas(u8 cols, u8 rows);

/*
 * @brief Writes a string to the canvas first char at x,y maximal length 29 chars
 *
 * @param row Row
 * @param column Column
 * @param attribute byte
 * 		 bit  7 must be 0
 * 		 bit  6 set to 1 for text to flash
 * 		 bits 2-5 must be 0
 * 		 bits 0-1 select font
 */
void dpWriteString(u8 row, u8 column, u8 attribute, const char *content);
