#pragma once
#include "fixedPointInt.h"
#include "typedefs.h"

/**
 * @brief A first order low pass filter
 *
 * @details This filter is a simple first order low pass filter with a rollover function. Internally, it uses fix32 variables
 */
class PT1 {
public:
	PT1() = default;
	/**
	 * @brief Construct a new PT1 object
	 *
	 * @param cutoffFreq Cutoff frequency of the filter
	 * @param sampleFreq Sample frequency of the filter (rate at which .update() is called)
	 */
	PT1(fix32 cutoffFreq, u32 sampleFreq);
	/**
	 * @brief Construct a new PT1 object
	 *
	 * @param alpha Alpha value of the filter, typically close to 0
	 */
	PT1(fix32 alpha);
	/**
	 * @brief provide a new value to the filter
	 *
	 * @param value The new value/sample to be filtered
	 * @return fix32 The filtered value
	 */
	inline const fix32 &update(fix32 value) {
		y = alpha * value + (fix32(1) - alpha) * y;
		return y;
	}
	/**
	 * @brief rollover if the value is out of bounds
	 *
	 * @return fix32 The new value after rollover
	 */
	fix32 rollover();
	/**
	 * @brief Set the Rollover Params object
	 *
	 * @param lowerBound The lower bound of the rollover (inclusive)
	 * @param upperBound The upper bound of the rollover (exclusive)
	 */
	void setRolloverParams(fix32 lowerBound, fix32 upperBound);
	/**
	 * @brief Set a new cutoff frequency for the filter, useful for dynamic filters
	 *
	 * @param cutoffFreq The new cutoff frequency
	 */
	void updateCutoffFreq(fix32 cutoffFreq);
	/**
	 * @brief Set a new alpha value for the filter, useful for dynamic filters
	 *
	 * @param alpha The new alpha value
	 */
	void updateAlpha(fix32 alpha);
	void set(fix32 value);
	void add(fix32 value) {
		y += value;
	}
	/// @brief Get the current value of the filter
	inline operator fix32() const { return y; }

	const fix32 &getConstRef() const { return y; }

private:
	fix32 alpha = 1; // close to 0, higher = less filtering
	fix32 y = 0;
	u32 sampleFreq = 100;
	fix32 lowerBound = 0;
	fix32 upperBound = 0;
	fix32 boundDiff = 0;
};

/**
 * @brief Uses two (actually three) PT1 filters to create a PT1 filter with a better condition when using large sample frequency to cutoff frequency ratios. Regular PT1 filters, due to being fix32, have precision issues when the sample frequency is much higher than the cutoff frequency.
 *
 * pt1a filters the input signal to remove high frequency noise so that doesn't get through to pt1b (average out the input for pt1b so that no samples are ignored).
 *
 * pt1b is updated at a lower rate, which is determined by the firstRatio parameter, and does the actual filtering with a low cutoff frequency.
 *
 * pt1c is updated at the same rate as pt1a, and smoothes out the output of pt1b to remove the temporal noise introduced by the low update rate of pt1b.
 *
 * Doesn't have advanced features like rollover
 */
class DualPT1 {
public:
	DualPT1() = default;
	DualPT1(fix32 cutoffFreq, u32 sampleFreq);

	inline const fix32 &update(fix32 value) {
		pt1a.update(value);
		if (++counter >= firstRatio) {
			counter = 0;
			pt1b.update(pt1a);
		}
		return pt1c.update(pt1b);
	}

	void set(fix32 value) {
		pt1a.set(value);
		pt1b.set(value);
		pt1c.set(value);
	}
	void add(fix32 value) {
		pt1a.add(value);
		pt1b.add(value);
		pt1c.add(value);
	}

	/// @brief Get the current value of the filter
	inline operator fix32() const { return pt1c; }

	const fix32 &getConstRef() const { return pt1c.getConstRef(); }

private:
	PT1 pt1a = PT1(1, 2); // input filter with high sample frequency, medium cutoff frequency
	PT1 pt1b = PT1(1, 2); // medium update rate, with actual (low) cutoff frequency
	PT1 pt1c = PT1(1, 2); // smoothes out the data from pt1b, high sample frequency, medium-low cutoff frequency
	u32 sampleFreq = 100;
	u32 firstRatio = 1;
	u32 counter = 0;
};

class PT2 {
public:
	PT2() = default;
	PT2(fix32 cutoffFreq, u32 sampleFreq);
	PT2(fix32 alpha);
	inline fix32 update(fix32 value) {
		y1 = y1 + alpha * (value - y1);
		y = y + alpha * (y1 - y);
		return y;
	}
	fix32 rollover();
	void setRolloverParams(fix32 lowerBound, fix32 upperBound);
	void updateCutoffFreq(fix32 cutoffFreq);
	void updateAlpha(fix32 alpha);
	void set(fix32 value);
	inline operator fix32() const { return y; }
	const fix32 &getConstRef() const { return y; }

private:
	fix32 alpha = 1;
	fix32 y = 0;
	fix32 y1 = 0;
	u32 sampleFreq = 100;
	fix32 lowerBound = 0;
	fix32 upperBound = 0;
	fix32 boundDiff = 0;
};

class PT3 {
public:
	PT3() = default;
	PT3(fix32 cutoffFreq, u32 sampleFreq);
	PT3(fix32 alpha);
	inline fix32 update(fix32 value) {
		y2 = y2 + alpha * (value - y2);
		y1 = y1 + alpha * (y2 - y1);
		y = y + alpha * (y1 - y);
		return y;
	}
	fix32 rollover();
	void setRolloverParams(fix32 lowerBound, fix32 upperBound);
	void updateCutoffFreq(fix32 cutoffFreq);
	void updateAlpha(fix32 alpha);
	void set(fix32 value);
	inline operator fix32() const { return y; };
	const fix32 &getConstRef() const { return y; }

private:
	fix32 alpha = 1;
	fix32 y = 0;
	fix32 y1 = 0;
	fix32 y2 = 0;
	u32 sampleFreq = 100;
	fix32 lowerBound = 0;
	fix32 upperBound = 0;
	fix32 boundDiff = 0;
};
