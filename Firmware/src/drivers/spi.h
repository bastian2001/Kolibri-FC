#include "Arduino.h"
#include "hardware/spi.h"
int regRead(spi_inst_t *spi, const uint cs, const uint8_t reg, uint8_t *buf, const uint16_t nbytes = 1, const uint16_t delay = 0, uint8_t dummy = true);
int regWrite(spi_inst_t *spi, const uint cs, const uint8_t reg, const uint8_t *buf, const uint16_t nbytes = 1, const uint16_t delay = 0);

void initDefaultSpi();