#include "fixedPointInt.h"
#include "typedefs.h"

class PT1 {
public:
	PT1(fix32 cutoffFreq, u32 sampleFreq);
	PT1(fix32 alpha);
	inline fix32 update(fix32 value) {
		y = y + alpha * (value - y);
		return y;
	}
	fix32 rollover();
	void setRolloverParams(fix32 lowerBound, fix32 upperBound); // lower bound inclusive, upper bound exclusive
	void updateCutoffFreq(fix32 cutoffFreq);
	void updateAlpha(fix32 alpha);
	inline operator fix32() const { return y; }

private:
	fix32 alpha;
	fix32 y = 0;
	u32 sampleFreq;
	fix32 lowerBound = 0;
	fix32 upperBound = 0;
	fix32 boundDiff  = 0;
};