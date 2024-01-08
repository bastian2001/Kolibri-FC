#include <Arduino.h>

extern int16_t baroASL, baroATO;
extern float baroPres;
extern uint8_t baroTemp;

void initBaro();

void baroLoop();