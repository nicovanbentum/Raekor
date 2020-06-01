#pragma once

namespace Raekor {
    class AsyncDispatcher {
        using task = std::function<void()>;

    public:
        AsyncDispatcher(int threadCount);
        ~AsyncDispatcher();

        void dispatch(const task& task);
        void handler();
        inline void wait() { while (activeTaskCount.load() > 0) {} }

        bool shouldQuit = false;
        
        std::mutex mutex;
        std::queue<task> queue;
        std::condition_variable cv;
        std::vector<std::thread> threads;
        std::atomic<uint32_t> activeTaskCount = 0;
    };
}