#include "Arduino.h"
#include <LittleFS.h>

extern bool littleFsReady;

extern File settingsFile;

void initLittleFs();

void openSettingsFile();

void closeSettingsFile();
