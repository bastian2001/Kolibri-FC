/// @brief checks for arming flags, sets flight mode, and checks for RX loss
void modesLoop();
/// @brief enables flight mode in OSD
void modesInit();

extern u32 armingDisableFlags; // each flag is 1 to prevent arming, or 0 to allow arming
extern bool armed; // true if the drone is armed
extern fix64 homepointLat, homepointLon; // GPS coordinates of the drone when it was armed
