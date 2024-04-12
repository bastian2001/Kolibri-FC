#include "typedefs.h"
extern u16 adcVoltage;
extern f32 adcCurrent;

/// @brief enables ADC pins for voltage and current monitoring, enables OSD elements
void initADC();

/// @brief reads the voltage, temperature and current from the ADC
void adcLoop();