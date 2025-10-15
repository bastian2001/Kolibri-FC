#pragma once

#include "serialhandler/msp.h"
#include "typedefs.h"
#include <Arduino.h>

/// @brief checks for arming flags, sets flight mode, and checks for RX loss
void modesLoop();
/// @brief enables flight mode in OSD
void modesInit();

enum class DisarmReason : u8 {
	UNSET = 0,
	SWITCH = 1,
	RXLOSS = 2,
	RTH_DONE = 3,
	LENGTH,
};

void disarm(DisarmReason reason = DisarmReason::LENGTH);

extern u32 armingDisableFlags; // each flag is 1 to prevent arming, or 0 to allow arming
extern volatile bool armed; // true if the drone is armed
extern fix64 homepointLat, homepointLon; // GPS coordinates of the drone when it was armed
extern fix32 homepointAlt; // altitude of the drone when it was armed
extern elapsedMillis armedTimer; // time since arm, unspecified during disarmed

void mspGetRxModes(u8 serialNum, MspVersion version);
void mspSetRxModes(u8 serialNum, MspVersion version, const char *reqPayload, u16 reqLen);

// explicitly numerated in case an option is being dropped => no need to change MSP command then
namespace RxModeIndex {
	enum {
		ARMED = 0,
		ANGLE = 1,
		ALT_HOLD = 2,
		GPS = 3,
		WAYPOINT = 4,
		BEEPER = 5,
		BB_HIGHLIGHT = 6,
		TUNING_NEXT_VAR = 7,
		TUNING_PREV_VAR = 8,
		TUNING_INC_VAL = 9,
		TUNING_DEC_VAL = 10,
		LENGTH
	};
};

class RxMode {
public:
	RxMode();

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

	/**
	 * @brief Gets the current channel and range values for this mode, but in the data saving i8 format.
	 *
	 * @param channel the aux channel (0-15) that this mode is using
	 * @param minRange the range's minimum for the channel, between -120 and 120
	 * @param maxRange the range's maximum for the channel, between -120 and 120
	 */
	void getValues8(i8 &channel, i8 &minRange, i8 &maxRange) const;

	/**
	 * @brief Sets the current channel and range values for this mode, but in the data saving i8 format.
	 *
	 * @param channel the aux channel (0-15) that this mode is using
	 * @param minRange the range's minimum for the channel, between -120 and 120
	 * @param maxRange the range's maximum for the channel, between -120 and 120
	 */
	void setValues8(i8 channel, i8 minRange, i8 maxRange);

	void disable();

private:
	i8 channel = -1; // channel number, -1 if not set
	u16 minRange = 2000; // minimum range for the channel, between 900 and 2100
	u16 maxRange = 2000; // maximum range for the channel, between 900 and 2100
};

extern RxMode rxModes[RxModeIndex::LENGTH];
