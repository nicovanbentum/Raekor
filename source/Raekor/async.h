#pragma once

namespace Raekor {

class Job {
public:
    using Ptr = std::shared_ptr<Job>;
    using Function = std::function<void()>;

    Job(const Function& inFunction) : m_Function(inFunction) {}
    void Run() { m_Function(); m_Finished = true; }
    void WaitCPU() const { while (!m_Finished) {} }

private:
    Function m_Function;
    bool m_Finished = false;
};


class ThreadPool {
private:

public:
    ThreadPool();
    ThreadPool(uint32_t threadCount);
    ~ThreadPool();
    
    using JobPtr = Job::Ptr;
    /* Queue up a job: lambda of signature void(void) */
    JobPtr QueueJob(const Job::Function& inJobFunction);

    /* Wait for all jobs to finish. */ 
    void WaitForJobs();

    /* Scoped lock on the global mutex,
        useful for ensuring thread safety inside a job function. */
    std::mutex& GetMutex() { return m_Mutex; }

    void SetActiveThreadCount(uint32_t inValue) { m_ActiveThreadCount = std::min(inValue, GetThreadCount()); }

    uint32_t GetThreadCount() { return uint32_t(m_Threads.size()); }
    int32_t  GetActiveJobCount() { return m_ActiveJobCount.load(); }

private:
    // per-thread function that waits on and executes tasks
    void ThreadLoop(uint32_t inThreadIndex);

    bool m_Quit = false;
    std::mutex m_Mutex;
    std::queue<JobPtr> m_JobQueue;
    std::vector<std::thread> m_Threads;
    std::atomic<int32_t> m_ActiveJobCount;
    std::atomic<uint8_t> m_ActiveThreadCount;
    std::condition_variable m_ConditionVariable;
};


extern ThreadPool g_ThreadPool;


}