#include "pch.h"
#include "timer.h"

namespace RK {

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
	return (float)( ( SDL_GetPerformanceCounter() - m_StartTime ) / (float)SDL_GetPerformanceFrequency() );
}


uint64_t Timer::sGetCurrentTick()
{
	return SDL_GetPerformanceCounter();
}


float Timer::sGetTicksToSeconds(uint64_t inTicks)
{
	return (float)( ( inTicks ) / (float)SDL_GetPerformanceFrequency() );
}


std::string Timer::GetElapsedFormatted()
{
	return std::to_string((float)( ( SDL_GetPerformanceCounter() - m_StartTime ) / (float)SDL_GetPerformanceFrequency() ));
}

} // raekor