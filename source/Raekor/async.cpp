#include "pch.h"
#include "async.h"

namespace Raekor {

ThreadPool g_ThreadPool;


ThreadPool::ThreadPool() : ThreadPool(std::thread::hardware_concurrency() - 1) {}


ThreadPool::ThreadPool(uint32_t inThreadCount) : m_ActiveThreadCount(inThreadCount)
{
	for (int i = 0; i < inThreadCount; i++)
	{
		m_Threads.push_back(std::thread(&ThreadPool::ThreadLoop, this, i));

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


ThreadPool::~ThreadPool()
{
	Shutdown();
}


Job::Ptr ThreadPool::QueueJob(const Job::Function& inFunction)
{
	auto job = std::make_shared<Job>(inFunction);

	{
		auto lock = std::scoped_lock<std::mutex>(m_Mutex);
		m_JobQueue.push(job);
	}

	m_ActiveJobCount.fetch_add(1);
	m_ConditionVariable.notify_one();

	return job;
}


void ThreadPool::WaitForJobs()
{
	m_ConditionVariable.notify_all();
	while (m_ActiveJobCount.load() > 0) {}
}


void ThreadPool::Shutdown()
{
	// let every thread know they can exit their while loops
	{
		auto lock = std::scoped_lock<std::mutex>(m_Mutex);
		m_Quit = true;
	}

	SetActiveThreadCount(m_Threads.size());

	m_ConditionVariable.notify_all();

	// wait for all to finish up
	for (auto& thread : m_Threads)
		if (thread.joinable())
			thread.join();
}


void ThreadPool::ThreadLoop(uint32_t inThreadIndex)
{
	// take control of the mutex
	auto lock = std::unique_lock<std::mutex>(m_Mutex);

	auto thread_index = inThreadIndex;

	do {
		// wait releases the mutex and re-acquires when there's work available
		m_ConditionVariable.wait(lock, [this, thread_index]
		{
			return ( m_JobQueue.size() || m_Quit ) && thread_index < m_ActiveThreadCount.load();
		});

		if (m_JobQueue.size() && !m_Quit)
		{
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