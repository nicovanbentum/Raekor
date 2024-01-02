#pragma once

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


class CPUProfilerSection
{
public:
	CPUProfilerSection(const char* inName) {  }
	~CPUProfilerSection() {  }

private:
	size_t mTimeIndex = 0;
	uint64_t mStartTick = 0;
	uint64_t mEndTick = 0;
};


#define PROFILE_SCOPED_FUNCTION_CPU(...) CPUProfilerSection TOKENPASTE2(cpu_profile_section_, __LINE__)(__FUNCTION__)

} // namespace Raekor