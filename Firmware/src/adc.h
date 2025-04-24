#include "typedefs.h"
extern u16 adcVoltage;
extern f32 adcCurrent;
extern u16 emptyVoltage;

/// @brief enables ADC pins for voltage and current monitoring, enables OSD elements
void initADC();

/// @brief reads the voltage, temperature and current from the ADC
void adcLoop();
