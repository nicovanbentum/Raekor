#include "pch.h"
#include "async.h"

namespace Raekor {

Async::Async() : Async(std::thread::hardware_concurrency() - 1) {}


Async::Async(int threadCount) {
    for (int i = 0; i < threadCount; i++) {
        m_Threads.push_back(std::thread(&Async::ThreadLoop, this));

        // Do Windows-specific thread setup:
        HANDLE handle = (HANDLE)m_Threads[i].native_handle();

        // Put each thread on to dedicated core:
        DWORD_PTR affinityMask = 1ull << i;
        DWORD_PTR affinity_result = SetThreadAffinityMask(handle, affinityMask);
        assert(affinity_result > 0);

        // Increase thread priority:
        BOOL priority_result = SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
        assert(priority_result != 0);
    }
}


Async::~Async() {
    // let every thread know they can exit their while loops
    {
        std::scoped_lock<std::mutex> lock(m_Mutex);
        m_Quit = true;
    }

    m_ConditionVariable.notify_all();

    // wait for all to finish up
    for (int i = 0; i < m_Threads.size(); i++)
        if (m_Threads[i].joinable())
            m_Threads[i].join();
}


Async::Job::Ptr Async::sQueueJob(const Job::Function& task) {
    auto job = std::make_shared<Job>(task);

    {
        std::scoped_lock<std::mutex> lock(global->m_Mutex);
        global->m_JobQueue.push(job);
    }

    global->m_ActiveJobCount.fetch_add(1);
    global->m_ConditionVariable.notify_one();

    return job;
}


std::scoped_lock<std::mutex> Async::sLock() {
    return std::scoped_lock<std::mutex>(global->m_Mutex);
}


void Async::sWait() {
    global->m_ConditionVariable.notify_all();
    while (global->m_ActiveJobCount.load() > 0) {}
}


void Async::sWait(const Job::Ptr& job) {
    while (!job->finished) {}
}


void Async::ThreadLoop() {
    // take control of the mutex
    auto lock = std::unique_lock<std::mutex>(m_Mutex);

    do {
        // wait releases the mutex and re-acquires when there's work available
        m_ConditionVariable.wait(lock, [this] {
            return m_JobQueue.size() || m_Quit;
        });

        if (m_JobQueue.size() && !m_Quit) {
            auto job = std::move(m_JobQueue.front());
            m_JobQueue.pop();

            lock.unlock();

            job->function();

            job->finished = true;

            // re-lock so wait doesn't unlock an unlocked mutex
            lock.lock();
            
            m_ActiveJobCount.fetch_sub(1);
        }

    } while (!m_Quit);
}

} // raekor