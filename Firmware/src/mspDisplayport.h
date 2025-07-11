#pragma once

#include "global.h"
#include <typedefs.h>

using namespace std;

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
 * @return message string for MSP-Displayport
 */
string dpWriteString(u8 row, u8 column, u8 attribute, string content);

/*
 * @brief updates digital OSD (write something first ^^)
 * @return message string for MSP-Displayport
 */
string dpShow();

/*
 *@brief clears digital OSD
 * @return message string for MSP-Displayport
 */
string dpClear();

void processMspDpMsg(const char *payload, u16 payloadLength, u8 serialNum, MspVersion MspVersion);
