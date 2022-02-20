#pragma once

namespace Raekor {

class Timer {
public:
    /* Creates and starts a Timer. */
    Timer();

    /* Restarts the timer and returns the elapsed time in seconds. */
    float Restart();

    /* Returns the elapsed time in seconds. */
    float GetElapsedTime();

    static float ToMilliseconds(float time) { return time * 1000; }
    static float ToMicroseconds(float time) { return time * 1'000'000; }

private:
    uint64_t startTime = 0;
};

} // namespace Raekor