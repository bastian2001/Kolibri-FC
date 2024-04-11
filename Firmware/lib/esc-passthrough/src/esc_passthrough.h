/*
 * Serial Communication
 * - for BlHeli
 */
#include "hardware/pio.h"
uint8_t processPassthrough(void);
void beginPassthrough(uint8_t pin, PIO pio = pio0, int8_t sm = -1);
void endPassthrough();
