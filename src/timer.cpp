#include "pch.h"
#include "timer.h"

#ifndef TIME_NOW
#define TIME_NOW() std::chrono::high_resolution_clock::now()
#endif

namespace Raekor {

void Timer::start() {
    running = true;
    startTime = TIME_NOW();
    stopTime = TIME_NOW();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

double Timer::stop() {
    running = false;
    stopTime = TIME_NOW();
    return std::chrono::duration<double, std::milli>(stopTime - startTime).count();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Timer::restart() {
    running = true;
    startTime = TIME_NOW();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

double Timer::elapsedMs() {
    if (running) {
        auto elapsed = std::chrono::duration<double, std::milli>(TIME_NOW() - startTime);
        return elapsed.count();
    }
    else {
        auto elapsed = std::chrono::duration<double, std::milli>(stopTime - startTime);
        return elapsed.count();
    }
}

} // raekor