#pragma once

namespace Raekor {
    class Async {
        using Task = std::function<void()>;

    public:
        Async();
        Async(int threadCount);
        ~Async();

        void dispatch(const Task& task);
        void handler();
        void wait();

        bool shouldQuit = false;
        
        std::mutex mutex;
        std::queue<Task> queue;
        std::condition_variable cv;
        std::vector<std::thread> threads;
        std::atomic<uint32_t> activeTaskCount = 0;
    };
}