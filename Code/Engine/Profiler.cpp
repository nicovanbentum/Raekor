#include "pch.h"
#include "Profiler.h"

namespace RK {

Profiler* g_Profiler = new Profiler();

void Profiler::Reset() 
{ 
	if (m_IsEnabled)
	{
		m_HistoryCPUSections = m_CPUSections;
		m_CPUSections.clear(); 
	}
}


CPUProfileSectionScoped::CPUProfileSectionScoped(const char* inName)
{
	if (g_Profiler->IsEnabled())
	{
		mIndex = g_Profiler->AllocateCPU();

		CPUProfileSection& section = g_Profiler->GetSectionCPU(mIndex);
		section.mStartTick = Timer::sGetCurrentTick();
		section.mName = inName;
		section.mDepth = g_Profiler->m_Depth++;
	}
}


CPUProfileSectionScoped::~CPUProfileSectionScoped()
{
	if (g_Profiler->IsEnabled())
	{
		CPUProfileSection& section = g_Profiler->GetSectionCPU(mIndex);
		section.mEndTick = Timer::sGetCurrentTick();
		g_Profiler->m_Depth--;
	}
}




} // raekor