#include "pch.h"
#include "async.h"

namespace Raekor {

Async::Async() : Async(std::thread::hardware_concurrency() - 1) {}



Async::Async(int threadCount) {
    for (int i = 0; i < threadCount; i++) {
        threads.push_back(std::thread(&Async::loop, this));

        // Do Windows-specific thread setup:
        HANDLE handle = (HANDLE)threads[i].native_handle();

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
        std::scoped_lock<std::mutex> lock(mutex);
        quit = true;
    }

    cv.notify_all();

    // wait for all to finish up
    for (int i = 0; i < threads.size(); i++) {
        if (threads[i].joinable()) {
            threads[i].join();
        }
    }
}



Async::Handle Async::dispatch(const Task& task) {
    auto dispatch = std::make_shared<Dispatch>(task);

    {
        std::scoped_lock<std::mutex> lock(async->mutex);
        async->queue.push(dispatch);
    }

    async->activeTaskCount.fetch_add(1);
    async->cv.notify_one();

    return dispatch;
}



std::scoped_lock<std::mutex> Async::lock() {
    return std::scoped_lock<std::mutex>(async->mutex);
}



void Async::wait() {
    while (async->activeTaskCount.load() > 0) {}
}



void Async::wait(const Handle& dispatch) {
    while (!dispatch->finished) {}
}



void Async::loop() {
    // take control of the mutex
    auto lock = std::unique_lock<std::mutex>(mutex);
    

    do {
        // wait releases the mutex and re-acquires when there's work available
        cv.wait(lock, [this] {
            return queue.size() || quit;
        });

        if (queue.size() && !quit) {
            auto dispatch = std::move(queue.front());
            queue.pop();

            lock.unlock();

            dispatch->task();

            dispatch->finished = true;

            // re-lock so wait doesn't unlock an unlocked mutex
            lock.lock();
            
            if(activeTaskCount.load() > 0) activeTaskCount.fetch_sub(1);
        }

    } while (!quit);
}

} // raekor