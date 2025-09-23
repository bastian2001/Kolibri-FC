#include "Arduino.h"
#include "drivers/halfduplexUart.h"
#include "ringbuffer.h"
#include "typedefs.h"

extern RingBuffer<u8> trampRxBuffer;

void trampInit();

void trampLoop();

u8 sendTrampUpdateMsg(u8 *buf);

u8 sendTrampConfigMsg(u8 *buf);
