#pragma once

namespace Raekor {

class Async {
    using Task = std::function<void()>;

public:
    Async();
    Async(int threadCount);
    ~Async();

    static void dispatch(const Task& task);
    static void lock(const Task& task) {
        std::scoped_lock<std::mutex> lock(async->mutex);
        task();
    }
    static void wait();

private:
    void handler();

    bool quit = false;
    std::mutex mutex;
    std::queue<Task> queue;
    std::condition_variable cv;
    std::vector<std::thread> threads;
    std::atomic<uint32_t> activeTaskCount = 0;

    static inline std::unique_ptr<Async> async = std::make_unique<Async>();
};

}