#pragma once
#include <typedefs.h>

#define HZ *10
namespace elemType {
	enum : u8 { // TODO find OSD elements to implement.
		UNDEFINED,
		// Battery info
		BATTERY_VOLTAGE,
		BATTERY_CELL_VOLLTAGE,
		BATTERY_CELL_COUNT,
		BATTERY_CURRENT,
		BATTERY_MAH_DRAWN,
		BATTERY_VOLTAGE_MIN,
		// GPS info
		GPS_LONGITUDE,
		GPS_LATITUDE,
		SPEED,
		ALTITUDE,
		HOME_DISTANCE,
		HOME_DIRECTION,
		// Flight info
		FLIGHT_MODE,
		RESCUE_STATUS,
		// Radio info
		RSSI_VAL,
		LINK_QUALITY,
		ELRS_RX_STATUS,
		// Sensors
		BARO_ALTITUDE,
		ESC_TEMP_0,
		ESC_TEMP_1,
		ESC_TEMP_2,
		ESC_TEMP_3,
		ESC_TEMP_AVG,
		IMU_ACCELERATION,
		IMU_PITCH,
		IMU_ROLL,
		IMU_YAW,
		// Stickpos
		TX_ROLL,
		TX_PITCH,
		TX_YAW,
		TX_THROTTLE,
		// Timers
		BAT_TIME,
		ARM_TIME,
		USER_TIME,
		// Warn
		ALARM_CRITICAL,
		ALARM_WARNING,
		ALARM_INFO
	};
}

class OsdElement {
public:
	/**
	 * @brief Create OSD Object
	 * @param ElemType What kind of element e.g. Battey voltage.
	 * @param layer priority low to high.
	 * @param in which profile is this element active.
	 */
	OsdElement(u8 element = elemType::UNDEFINED, u8 layer = 0, u8 profile = 0) {
		this->element = element;
		this->layer = layer;
		this->profile = profile;
		setRefreshRate(20);
		this->row = 0;
		this->column = 0;
		blinking = false;
		rawDataPtr = nullptr;
		screenText = new char[32]();
		lastUpdateMillis = 0xFFFFFFFF; // Has not been updated yet -> max value
	}
	/**
	 * @brief Delete OSD object and remove from OSD if it already has been drawn
	 */
	~OsdElement() {
		// TODO Clear deleted element from OSD.
	}
	/**
	 * @brief sets the position of the element on the screen
	 *  @param row The row position
	 *  @param column The column position
	 */
	void setPos(u8 row, u8 column);
	/**
	 * @brief draws the element on the OSD
	 *  @note This should only be called if the element is scheduled to be drawn.
	 */
	void drawOsdElement();
	/**
	 * @brief checks if the element is scheduled to be drawn
	 * @return true if the element is scheduled to be drawn, false otherwise
	 */
	bool isScheduled(); //? maybe return timestamp to reduce if statements
	/**
	 * @brief updates the data of the element
	 * @note This is used by the OSD handler to update the data of the element.
	 */
	void updateOsdElementData();
	/**
	 * @brief sets the pointer to the data that should be displayed
	 * @param ptr Pointer to the data that should be displayed
	 * @note This should be called in the part of the code, that updates the data. Ideally not repeatedly.
	 */
	void setRawDataPtr(void *ptr);
	/**
	 * @brief returns the refresh rate in milliseconds
	 * @return period time in milliseconds
	 */
	u32 getRefreshMillis();
	/**
	 * @brief returns the type of the element
	 * @return the type of the element as defined in elemType
	 */
	u8 getElementType() const;
	/**
	 * @brief sets the refreshrate of the Element (automatically caps at the maximum supported by the OSD)
	 * @param refreshRate Refresh rate in Hz
	 */
	void setRefreshRate(u8 refreshRate);

private:
	/**
	 * @brief clears the element data
	 */
	void clear(); // should only be necessary internally //TODO Check if necessary. Remove or implement.
	bool updated;
	u8 element;
	u8 layer; // TODO Implement
	u8 profile;
	u8 row;
	u8 column;
	bool blinking;
	u32 lastUpdateMillis;
	u32 refreshMillis;
	void *rawDataPtr; // Where to find the data
	char *screenText; // What is displayed on screen
};
