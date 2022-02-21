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

    /* Queue up a dispatch: lambda of signature void(void) */
    static Handle sDispatch(const Task& task);

    /* Wait for all dispatches to finish. */ 
    static void sWait();

    /* Wait for a specific dispatch to finish. */
    static void sWait(const Handle& dispatch);

    /* Scoped lock on the global mutex,
        useful for ensuring thread safety inside a dispatch. */
    [[nodiscard]] static std::scoped_lock<std::mutex> sLock();

private:
    // per-thread function that waits on and executes tasks
    void ThreadLoop();

    bool m_Quit = false;
    std::mutex m_Mutex;
    std::condition_variable m_CondVar;
    std::vector<std::thread> m_Threads;
    std::atomic<uint32_t> m_ActiveDispatchCount = 0;
    std::queue<std::shared_ptr<Dispatch>> m_DispatchQueue;

    // global singleton
    static inline std::unique_ptr<Async> global = std::make_unique<Async>();
};

}