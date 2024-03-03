#pragma once

#include "timer.h"

namespace Raekor {

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

	int AllocateCPU() { m_CPUSections.emplace_back(); return m_CPUSections.size() - 1; }
	CPUProfileSection& GetSectionCPU(int inIndex) { return m_CPUSections[inIndex]; }
	Slice<CPUProfileSection> GetCPUProfileSections() const { return Slice(m_HistoryCPUSections); }

private:
	int m_Depth = 0;
	bool m_IsEnabled = true;
	std::vector<CPUProfileSection> m_CPUSections;
	std::vector<CPUProfileSection> m_HistoryCPUSections;
};


extern Profiler* g_Profiler;


class CPUProfileSectionScoped
{
public:
	CPUProfileSectionScoped(const char* inName);
	~CPUProfileSectionScoped();

private:
	int mIndex = 0;
};

}