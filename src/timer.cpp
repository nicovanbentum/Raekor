#include "pch.h"
#include "timer.h"

namespace Raekor {

void Timer::start() {
    running = true;
    startTime = SDL_GetPerformanceCounter();
    stopTime = SDL_GetPerformanceCounter();
}



double Timer::stop() {
    running = false;
    stopTime = SDL_GetPerformanceCounter();
    return (double)((stopTime - startTime) * 1000 / (double)SDL_GetPerformanceFrequency() );
}



void Timer::restart() {
    running = true;
    startTime = SDL_GetPerformanceCounter();
}



double Timer::elapsedMs() {
    if (running) {
        return (double)((SDL_GetPerformanceCounter() - startTime) * 1000 / (double)SDL_GetPerformanceFrequency() );
    }
    else {
        return (double)((stopTime - startTime) * 1000 / (double)SDL_GetPerformanceFrequency() );
    }
}

} // raekor