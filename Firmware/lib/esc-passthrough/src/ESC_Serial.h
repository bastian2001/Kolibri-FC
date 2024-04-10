void InitSerialOutput(void);
void DeinitSerialOutput(void);
void SendESC(uint8_t tx_buf[], uint16_t buf_size, bool CRC = true);
uint16_t ByteCrc(uint8_t data, uint16_t crc);
uint16_t GetESC(uint8_t rx_buf[], uint16_t wait_ms);
#include "../../../src/typedefs.h"
void delayWhileRead(u16 ms);
void pioResetESC();
