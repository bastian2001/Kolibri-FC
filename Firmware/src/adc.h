#include "typedefs.h"
extern u16 adcVoltage;
extern f32 adcCurrent;

extern u8 batCells;
extern u8 batState; // 0 = no battery (USB or startup), 1 = battery found
extern u8 cellCountSetting;
extern u16 emptyVoltageSetting;

/// @brief enables ADC pins for voltage and current monitoring, enables OSD elements
void initADC();

/// @brief reads the voltage, temperature and current from the ADC
void adcLoop();
