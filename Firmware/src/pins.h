#pragma once
#include "typedefs.h"

bool sendIoConstraints(u8 page, char *buf, u16 &len);
bool pinIsAllowed(u8 pin);
bool pinIsRecommended(u8 pin);
bool pinHasHwUart(u8 pin, u8 uart, bool tx);
