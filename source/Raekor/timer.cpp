#include "pch.h"
#include "timer.h"

namespace Raekor {

Timer::Timer() {
    m_StartTime = SDL_GetPerformanceCounter();
}


float Timer::Restart() {
    float time = GetElapsedTime();
    m_StartTime = SDL_GetPerformanceCounter();
    return time;
}


float Timer::GetElapsedTime() {
    return (float)((SDL_GetPerformanceCounter() - m_StartTime) / (float)SDL_GetPerformanceFrequency());
}

} // raekor