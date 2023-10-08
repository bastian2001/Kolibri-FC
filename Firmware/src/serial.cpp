#include "global.h"

uint8_t         readChar   = 0;
HardwareSerial *serials[3] = {
    &Serial,
    &Serial1,
    &Serial2};

// 0 = serial, 1 = serial1, 2 = serial2
uint32_t serialFunctions[3] = {
    SERIAL_CONFIGURATOR,
    SERIAL_DISABLED,
    SERIAL_DISABLED};

void serialLoop() {
    for (int i = 0; i < 3; i++) {
        if (serialFunctions[i] & SERIAL_DISABLED)
            continue;
        HardwareSerial *serial = &Serial;
        if (!serial->available())
            continue;
        readChar = serial->read();
        if (serialFunctions[i] & SERIAL_CONFIGURATOR) {
            configuratorHandleByte(readChar, i);
        }
    }
}