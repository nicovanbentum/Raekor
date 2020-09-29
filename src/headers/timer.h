#pragma once

#define TIMER_SECTION_START() Timer timer; timer.start()
#define TIMER_SECTION_END(s) timer.stop(); std::cout << s << timer.elapsedMs() << '\n'

namespace Raekor {

class Timer {
public:
    Timer() {}
    void start();
    double stop();
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

//////////////////////////////////////////////////////////////////////////////////////////////////

class ScopedTimer : Timer {
public:
    ScopedTimer(const std::string& name) : name(name) {
        start();
    }
    ~ScopedTimer() {
        stop();
        std::cout << "Timer " << name << " elapsed " << elapsedMs() << " ms" << '\n';
    }

private:
    std::string name;
};

} // namespace Raekor