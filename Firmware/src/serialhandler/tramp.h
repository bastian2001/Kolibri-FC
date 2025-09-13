#include "Arduino.h"
#include "drivers/halfduplexUart.h"
#include "ringbuffer.h"
#include "typedefs.h"

extern SerialOnewire SoftSerial1;
extern RingBuffer<u8> trampRxBuffer;
