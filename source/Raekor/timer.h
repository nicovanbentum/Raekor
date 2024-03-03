#pragma once

#include "slice.h"

namespace Raekor {

class Timer
{
public:
	/* Creates and starts a Timer. */
	Timer();

	/* Restarts the timer and returns the elapsed time in seconds. */
	float Restart();

	/* Returns the elapsed time in seconds. */
	float GetElapsedTime();

	/* Returns the elapsed time in seconds. */
	std::string GetElapsedFormatted();

	static uint64_t sGetCurrentTick();
	static float sGetTicksToSeconds(uint64_t inTicks);

	static float sToMilliseconds(float inTime) { return inTime * 1000; }
	static float sToMicroseconds(float inTime) { return inTime * 1'000'000; }

private:
	uint64_t m_StartTime = 0;
};

} // namespace Raekor