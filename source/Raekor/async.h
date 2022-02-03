#pragma once

namespace Raekor {

class Async {
private:
    using Task = std::function<void()>;

    struct Dispatch {
        Dispatch(const Task& task) : task(task) {}

        Task task;
        bool finished = false;
    };

public:
    // type returned when dispatching a lambda
    using Handle = std::shared_ptr<Dispatch>;

    Async();
    Async(int threadCount);
    ~Async();

    // queue up a random lambda of signature void(void)
    static Handle dispatch(const Task& task);

    // wait for all tasks to finish or a specific dispatch
    static void wait();
    static void wait(const Handle& dispatch);

    // scoped lock on the global mutex, 
    // useful for ensuring thread safety inside a dispatch
    [[nodiscard]] static std::scoped_lock<std::mutex> lock();

private:
    // per-thread function that waits on and executes tasks
    void loop();

    bool quit = false;
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::thread> threads;
    std::atomic<uint32_t> activeTaskCount = 0;
    std::queue<std::shared_ptr<Dispatch>> queue;

    // singleton
    static inline std::unique_ptr<Async> async = std::make_unique<Async>();
};

}