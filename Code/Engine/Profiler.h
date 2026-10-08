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

	float GetSeconds() const { return Timer::sGetTicksToSeconds(mEndTick - mStartTick); }
};


class Profiler
{
public:
	friend class CPUProfileSectionScoped;

	void Reset();

	bool IsEnabled() const { return m_IsEnabled; }
	void SetEnabled(bool inEnabled) { m_IsEnabled = inEnabled; }

	int BeginCPU(const char* inName);
	void EndCPU(int inIndex, uint64_t inFrame);

	const char* InternName(const char* inName);

	void SetGPUSections(Slice<const ProfileSection> inSections) { m_GPUSections.assign(inSections.begin(), inSections.end()); }

	uint64_t GetFrame() const { return m_Frame; }
	const Array<ProfileSection>& GetCPUSections() const { return m_HistoryCPUSections; }
	const Array<ProfileSection>& GetGPUSections() const { return m_GPUSections; }

protected:
	int m_Depth = 0;
	bool m_IsEnabled = true;
	uint64_t m_Frame = 0;
	std::thread::id m_ThreadID = std::this_thread::get_id();

	Array<ProfileSection> m_CPUSections;
	Array<ProfileSection> m_HistoryCPUSections;
	Array<ProfileSection> m_GPUSections;

	Mutex m_NamesMutex;
	HashSet<String> m_Names;
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
