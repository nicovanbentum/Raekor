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

    m_CondVar.notify_all();

    // wait for all to finish up
    for (int i = 0; i < m_Threads.size(); i++) {
        if (m_Threads[i].joinable()) {
            m_Threads[i].join();
        }
    }
}


Async::Handle Async::sDispatch(const Task& task) {
    auto dispatch = std::make_shared<Dispatch>(task);

    {
        std::scoped_lock<std::mutex> lock(global->m_Mutex);
        global->m_DispatchQueue.push(dispatch);
    }

    global->m_ActiveDispatchCount.fetch_add(1);
    global->m_CondVar.notify_one();

    return dispatch;
}


std::scoped_lock<std::mutex> Async::sLock() {
    return std::scoped_lock<std::mutex>(global->m_Mutex);
}


void Async::sWait() {
    while (global->m_ActiveDispatchCount.load() > 0) {}
}


void Async::sWait(const Handle& dispatch) {
    while (!dispatch->finished) {}
}


void Async::ThreadLoop() {
    // take control of the mutex
    auto lock = std::unique_lock<std::mutex>(m_Mutex);
    

    do {
        // wait releases the mutex and re-acquires when there's work available
        m_CondVar.wait(lock, [this] {
            return m_DispatchQueue.size() || m_Quit;
        });

        if (m_DispatchQueue.size() && !m_Quit) {
            auto dispatch = std::move(m_DispatchQueue.front());
            m_DispatchQueue.pop();

            lock.unlock();

            dispatch->task();

            dispatch->finished = true;

            // re-lock so wait doesn't unlock an unlocked mutex
            lock.lock();
            
            if(m_ActiveDispatchCount.load() > 0) m_ActiveDispatchCount.fetch_sub(1);
        }

    } while (!m_Quit);
}

} // raekor