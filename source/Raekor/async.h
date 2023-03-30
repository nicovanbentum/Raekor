#pragma once

namespace Raekor {

class Async {
private:
    struct Job {
        using Ptr = std::shared_ptr<Job>;
        using Function = std::function<void()>;

        Job(const Function& function) : function(function) {}
        void WaitCPU() const { while (!finished) {} }

        Function function;
        bool finished = false;
    };

public:
    Async();
    Async(int threadCount);
    ~Async();
    
    /* Queue up a job: lambda of signature void(void) */
    static Job::Ptr sQueueJob(const Job::Function& function);

    /* Wait for all jobs to finish. */ 
    static void sWait();

    /* Wait for a specific job to finish. */
    static void sWait(const Job::Ptr& job);

    /* Scoped lock on the global mutex,
        useful for ensuring thread safety inside a job function. */
    [[nodiscard]] static std::scoped_lock<std::mutex> sLock();

    static std::mutex& sGetMutex() { return global->m_Mutex; }

    static int32_t sActiveJobCount() { return global->m_ActiveJobCount.load(); }
    static uint32_t sNumberOfThreads() { return uint32_t(global->m_Threads.size()); }

private:
    // per-thread function that waits on and executes tasks
    void ThreadLoop();

    bool m_Quit = false;
    std::mutex m_Mutex;
    std::queue<Job::Ptr> m_JobQueue;
    std::vector<std::thread> m_Threads;
    std::atomic<int32_t> m_ActiveJobCount;
    std::condition_variable m_ConditionVariable;

    // global singleton
    static inline std::unique_ptr<Async> global = std::make_unique<Async>();
};

}