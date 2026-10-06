#include "pch.h"
#include "Profiler.h"

namespace RK {

Profiler* g_Profiler = new Profiler();

void Profiler::Reset() 
{ 
	if (std::this_thread::get_id() != m_ThreadID)
		return;

	if (m_IsEnabled)
	{
		m_HistoryCPUSections.clear();

		for (const CPUProfileSection& section : m_CPUSections)
		{
			if (section.mEndTick != 0)
				m_HistoryCPUSections.push_back(section);
		}
	}

	m_CPUSections.clear();
	m_Depth = 0;
	m_Frame++;
}


int Profiler::BeginCPU(const char* inName)
{
	if (std::this_thread::get_id() != m_ThreadID || !m_IsEnabled)
		return -1;

	CPUProfileSection& section = m_CPUSections.emplace_back();
	section.mName = inName;
	section.mDepth = m_Depth++;
	section.mStartTick = Timer::sGetCurrentTick();

	return int(m_CPUSections.size() - 1);
}


void Profiler::EndCPU(int inIndex, uint64_t inFrame)
{
	if (inIndex < 0 || inFrame != m_Frame || inIndex >= int(m_CPUSections.size()))
		return;

	m_CPUSections[inIndex].mEndTick = Timer::sGetCurrentTick();
	m_Depth = glm::max(m_Depth - 1, 0);
}


CPUProfileSectionScoped::CPUProfileSectionScoped(const char* inName)
{
	mIndex = g_Profiler->BeginCPU(inName);

	if (mIndex >= 0)
		mFrame = g_Profiler->GetFrame();
}


CPUProfileSectionScoped::~CPUProfileSectionScoped()
{
	g_Profiler->EndCPU(mIndex, mFrame);
}


} // raekor