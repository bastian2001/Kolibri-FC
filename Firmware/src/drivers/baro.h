#include <Arduino.h>

extern i16 baroASL, baroATO;
extern f32 baroPres;
extern u8 baroTemp;

void initBaro();

void baroLoop();