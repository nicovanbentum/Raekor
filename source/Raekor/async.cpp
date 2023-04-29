#include "pch.h"
#include "async.h"

namespace Raekor {

Async::Async() : Async(std::thread::hardware_concurrency() - 1) {}


Async::Async(uint32_t inThreadCount) {
    for (int i = 0; i < inThreadCount; i++) {
        m_Threads.push_back(std::thread(&Async::ThreadLoop, this));

        // Do Windows-specific thread setup:
        HANDLE handle = (HANDLE)m_Threads[i].native_handle();

        const auto thread_name = L"JobThread " + std::to_wstring(i);
        SetThreadDescription(handle, thread_name.c_str());

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
        auto lock = std::scoped_lock<std::mutex>(m_Mutex);
        m_Quit = true;
    }

    m_ConditionVariable.notify_all();

    // wait for all to finish up
    for (auto& thread : m_Threads)
        if (thread.joinable())
            thread.join();
}


Async::Job::Ptr Async::sQueueJob(const Job::Function& inFunction) {
    auto job = std::make_shared<Job>(inFunction);

    {
        auto lock = std::scoped_lock<std::mutex>(global->m_Mutex);
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

            job->Run();

            // re-lock so wait doesn't unlock an unlocked mutex
            lock.lock();
            
            m_ActiveJobCount.fetch_sub(1);
        }

    } while (!m_Quit);
}

} // raekor