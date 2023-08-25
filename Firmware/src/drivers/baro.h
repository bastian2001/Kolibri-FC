#include <Arduino.h>

#define BARO_I2C_ADDR 0x78

extern int16_t baroASL, baroATO;
extern float baroPres;
extern uint8_t baroTemp;

void initBaro();

void startBaroMeasure();

bool checkBaroFinished();

void baroUpdate();

void baroSetOffset();

void baroLoop();