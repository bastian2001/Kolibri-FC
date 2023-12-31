#include <Arduino.h>

enum class OSDReg : uint8_t {
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
	RB0	  = 0x10,
	OSDBL = 0x6C,
	STAT  = 0xA0,
	DMDO  = 0xB0,
	CMDO  = 0xC0,
};

#define OSD_WIDTH 30
#define OSD_HEIGHT 16
#define OSD_MAX_ELEM 128

enum class OSDElem : uint8_t {
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
	GROUND_SPEED
};

void osdInit();

void osdLoop();

void updateElem(OSDElem elem, const char *str);

void placeElem(OSDElem elem, uint8_t x, uint8_t y);

void enableElem(OSDElem elem);
void disableElem(OSDElem elem);

void disableOSD();
void enableOSD();

void updateCharacter(uint8_t cmAddr, uint8_t data[54]);