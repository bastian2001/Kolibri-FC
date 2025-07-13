#include "typedefs.h"
#include <Arduino.h>

/// @brief checks for arming flags, sets flight mode, and checks for RX loss
void modesLoop();
/// @brief enables flight mode in OSD
void modesInit();

extern u32 armingDisableFlags; // each flag is 1 to prevent arming, or 0 to allow arming
extern bool armed; // true if the drone is armed
extern i32 startPointLat, startPointLon; // GPS coordinates of the drone when it was armed

void mspGetRxModes(u8 serialNum, MspVersion version);
void mspSetRxModes(u8 serialNum, MspVersion version, const char *reqPayload, u16 reqLen);

namespace RxModeIndex {
	enum {
		ARMED = 0,
		ANGLE = 1,
		ALT_HOLD = 2,
		GPS = 3,
		WAYPOINT = 4,
		BEEPER = 5,
		BB_HIGHLIGHT = 6,
		LENGTH
	};
};

class RxMode {
public:
	RxMode() = default;

	/**
	 * @brief Checks if the mode is active based on the current channel value.
	 *
	 * @return true if the mode is active, false otherwise or if not set up
	 */
	bool isActive() const;

	/**
	 * @brief Checks if the mode is configured, meaning it has a valid channel
	 *
	 * @return true if the mode is configured, false otherwise
	 */
	bool isConfigured() const;

	/**
	 * @brief Sets the channel for this mode
	 *
	 * @param channel the aux channel (0-15) that this mode shall use
	 */
	void setChannel(i8 channel);

	/**
	 * @brief Sets the range for this mode, where each aux channel goes from 1000 to 2000.
	 *
	 * The range can be set 900-2100. Swaps min and max if min is greater than max. Floored to steps of 5.
	 *
	 * @param minRange the range's minimum for the channel, between 900 and 2100
	 * @param maxRange the range's maximum for the channel, between 900 and 2100
	 */
	void setRange(u16 minRange, u16 maxRange);

	/**
	 * @brief Gets the current channel and range values for this mode.
	 *
	 * @param channel the aux channel (0-15) that this mode is using
	 * @param minRange the range's minimum for the channel, between 900 and 2100
	 * @param maxRange the range's maximum for the channel, between 900 and 2100
	 */
	void getValues(i8 &channel, u16 &minRange, u16 &maxRange) const;

	void disable() {
		channel = -1; // disable the mode
		minRange = 255; // reset to default minimum range
		maxRange = 255; // reset to default maximum range
	}

private:
	i8 channel = -1; // channel number, -1 if not set
	u16 minRange = 2000; // minimum range for the channel, between 900 and 2100
	u16 maxRange = 2000; // maximum range for the channel, between 900 and 2100
};

extern RxMode rxModes[RxModeIndex::LENGTH];
