#include "pch.h"
#include "timer.h"

namespace Raekor {

Timer::Timer() {
    startTime = SDL_GetPerformanceCounter();
}

float Timer::Restart() {
    float time = GetElapsedTime();
    startTime = SDL_GetPerformanceCounter();
    return time;
}

float Timer::GetElapsedTime() {
    return (float)((SDL_GetPerformanceCounter() - startTime) / (float)SDL_GetPerformanceFrequency());
}

} // raekor