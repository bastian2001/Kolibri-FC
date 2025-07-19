#include <Arduino.h>

enum class OSDReg : u8 {
	VM0,
	VM1,
	HOS,
	VOS,
	DMM,
	DMAH,
	DMAL,
	DMDI,
	CMM,
	CMAH,
	CMAL,
	CMDI,
	OSDM,
	RB0 = 0x10,
	OSDBL = 0x6C,
	STAT = 0xA0,
	DMDO = 0xB0,
	CMDO = 0xC0,
};

#define OSD_WIDTH 30
#define OSD_HEIGHT 16
#define OSD_MAX_ELEM 128

enum class OSDElem : u8 {
	WARNINGS,
	TOT_VOLTAGE,
	CURRENT,
	FLIGHT_MODE,
	ALTITUDE,
	LATITUDE,
	LONGITUDE,
	GPS_STATUS,
	HEADING,
	HOME_DISTANCE,
	GROUND_SPEED,
	PLUS_CODE,
	CELL_VOLTAGE
};

void osdInit();

void osdLoop();

void updateElem(OSDElem elem, const char *str);

void placeElem(OSDElem elem, u8 x, u8 y);

void enableBlinking(OSDElem elem);
void disableBlinking(OSDElem elem);

void enableElem(OSDElem elem);
void disableElem(OSDElem elem);

void disableOSD();
void enableOSD();

void updateCharacter(u8 cmAddr, u8 data[54]);
