#include "pch.h"
#include "timer.h"

namespace RK {

uint64_t GetCPUFrequency()
{
    static uint64_t frequency = SDL_GetPerformanceFrequency();
    return frequency;
}

Timer::Timer()
{
	m_StartTime = SDL_GetPerformanceCounter();
}


float Timer::Restart()
{
	float time = GetElapsedTime();
	m_StartTime = SDL_GetPerformanceCounter();
	return time;
}


float Timer::GetElapsedTime()
{
	return (float)( ( SDL_GetPerformanceCounter() - m_StartTime ) / (float)GetCPUFrequency() );
}


uint64_t Timer::sGetCurrentTick()
{
	return SDL_GetPerformanceCounter();
}


float Timer::sGetTicksToSeconds(uint64_t inTicks)
{
    static const uint64_t frequency = GetCPUFrequency();
	return (float)( ( inTicks ) / (float)frequency );
}


std::string Timer::GetElapsedFormatted()
{
	return std::to_string((float)( ( SDL_GetPerformanceCounter() - m_StartTime ) / (float)GetCPUFrequency() ));
}

} // raekor