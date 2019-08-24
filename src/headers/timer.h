#pragma once

namespace Raekor {

class Timer {
public:
    Timer() {}
    void start();
    void stop();
    void restart();
    double elapsed_ms();

private:
    bool running = false;
#ifdef _WIN32
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    std::chrono::time_point<std::chrono::steady_clock> stop_time;
#else 
    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> stop_time;
#endif
};

} // namespace Raekor