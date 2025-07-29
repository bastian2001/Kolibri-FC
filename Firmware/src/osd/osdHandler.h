#pragma once
#include "global.h"
#include "osdElement.h"
#include <typedefs.h>

#define MAX_OSD_ELEMENTS 256
#define OSD_TIMEOUT 60000 // Timeout in milliseconds until OSD is considered as not available
#define CHUNKSIZE 8
#define MAXIMUM_TIMEOUT_MILLISECONDS 5000

class OsdHandler {
public:
	/**
	 * @brief Get singleton instance of OsdHandler
	 * @return Instance
	 */
	static OsdHandler &get() {
		static OsdHandler osdHandler;
		return osdHandler;
	}

	/**
	 * @brief Initialize the OSD handler
	 */
	void init();
	/**
	 * @brief called in loop
	 */
	void osdHandlerLoop();
	/**
	 * @brief Add osd elements to handler
	 * @note After finishing adding elements optimize() is called. //TODO Have a way to add multiple elements without optimize() running everytime
	 */
	void addOsdElement(OsdElement *element);
	/**
	 * @brief Find a OSD element of a given type e.g. BATTERY_VOLTAGE
	 * @return Index where the element is in the elements array.//TODO Change how this is handled in the furute
	 */
	int find(ElementType elementType);
	OsdElement *elements[MAX_OSD_ELEMENTS] = {nullptr};

private:
	OsdHandler() {
		lastCall = 0;
	}
	~OsdHandler() = default;
	OsdHandler(const OsdHandler &) = delete;
	void operator=(const OsdHandler &) = delete;
	/**
	 * @brief Optimize the OSD element array, so osdHandlerLoop() dosn't take too long.
	 */
	void optimize();
	u32 minTimeout;
	u16 lastElem;
	u8 chunk;
	u8 lastChunk;
	elapsedMillis lastCall;
	enum OsdType : u8 {
		NONE,
		DIGITAL,
		ANALOG,
		HYBRID
	};
	OsdType osdtype = OsdType::NONE;
};

extern OsdHandler osdHandler;
