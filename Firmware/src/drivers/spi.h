#include "Arduino.h"
#include "hardware/spi.h"
int regRead(spi_inst_t *spi, const uint cs, const u8 reg, u8 *buf, const u16 nbytes = 1, const u16 delay = 0, u8 dummy = true);
int regWrite(spi_inst_t *spi, const uint cs, const u8 reg, const u8 *buf, const u16 nbytes = 1, const u16 delay = 0);

void initDefaultSpi();