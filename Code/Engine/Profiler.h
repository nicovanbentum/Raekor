#pragma once

#include "timer.h"

namespace RK {

#define PROFILE_SCOPE_CPU(name) CPUProfileSectionScoped TOKENPASTE2(cpu_profile_section_, __LINE__)(name)

#define PROFILE_FUNCTION_CPU() CPUProfileSectionScoped TOKENPASTE2(cpu_profile_section_, __LINE__)(__FUNCTION__)


struct ProfileSection
{
	uint32_t mDepth = 0;
	uint64_t mEndTick = 0;
	uint64_t mStartTick = 0;
	const char* mName = nullptr;

	virtual float GetSeconds() const = 0;
};

struct CPUProfileSection : public ProfileSection
{
	float GetSeconds() const final { return Timer::sGetTicksToSeconds(mEndTick - mStartTick); }
};

class Profiler
{
public:
	friend class CPUProfileSection;
	friend class CPUProfileSectionScoped;

	void Reset();

	bool IsEnabled() const { return m_IsEnabled; }
	void SetEnabled(bool inEnabled) { m_IsEnabled = inEnabled; }

	int BeginCPU(const char* inName);
	void EndCPU(int inIndex, uint64_t inFrame);

	uint64_t GetFrame() const { return m_Frame; }
	const Array<CPUProfileSection>& GetCPUProfileSections() const { return m_HistoryCPUSections; }

protected:
	int m_Depth = 0;
	bool m_IsEnabled = true;
	uint64_t m_Frame = 0;
	std::thread::id m_ThreadID = std::this_thread::get_id();

	Array<CPUProfileSection> m_CPUSections;
	Array<CPUProfileSection> m_HistoryCPUSections;
};


extern RK_API Profiler* g_Profiler;


class CPUProfileSectionScoped
{
public:
	CPUProfileSectionScoped(const char* inName);
	~CPUProfileSectionScoped();

private:
	int mIndex = -1;
	uint64_t mFrame = 0;
};

}