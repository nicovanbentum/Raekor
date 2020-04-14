#pragma once

namespace Raekor {

class Timer {
public:
    Timer() {}
    void start();
    void stop();
    void restart();
    double elapsedMs();

private:
    bool running = false;
#ifdef _WIN32
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    std::chrono::time_point<std::chrono::steady_clock> stopTime;
#else 
    std::chrono::time_point<std::chrono::system_clock> startTime;
    std::chrono::time_point<std::chrono::system_clock> stopTime;
#endif
};

} // namespace Raekor