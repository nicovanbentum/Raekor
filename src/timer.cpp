#include "pch.h"
#include "timer.h"

#ifndef TIME_NOW
#define TIME_NOW() std::chrono::high_resolution_clock::now()
#endif

namespace Raekor {

void Timer::start() {
    running = true;
    start_time = TIME_NOW();
    stop_time = TIME_NOW();
}

void Timer::stop() {
    running = false;
    stop_time = TIME_NOW();
}

void Timer::restart() {
    running = true;
    start_time = TIME_NOW();
}

double Timer::elapsed_ms() {
    if (running) {
        auto elapsed = std::chrono::duration<double, std::milli>(TIME_NOW() - start_time);
        return elapsed.count();
    }
    else {
        auto elapsed = std::chrono::duration<double, std::milli>(stop_time - start_time);
        return elapsed.count();
    }
}

} // namespace Raekor