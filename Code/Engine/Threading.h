#pragma once

namespace RK {

class Job
{
public:
	using Ptr = std::shared_ptr<Job>;
	using Function = std::function<void()>;

	Job(const Function& inFunction) : m_Function(inFunction) {}
	void Run() { m_Function(); m_Finished = true; }
	void WaitCPU() const { while (!m_Finished) {} }

	class Barrier
	{
	public:
		Barrier(uint32_t inJobCount) { m_Jobs.reserve(inJobCount); }
		void AddJob(Job::Ptr inJob) { m_Jobs.push_back(inJob); }
		void Wait() const;

	private:
		Array<Job::Ptr> m_Jobs;
	};

private:
	Function m_Function;
	Atomic<bool> m_Finished = false;
};


class ThreadPool
{
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

	/* exits all the threads. */
	void Shutdown();

	/* Scoped lock on the global mutex,
		useful for ensuring thread safety inside a job function. */
	Mutex& GetMutex() { return m_Mutex; }

	void SetActiveThreadCount(uint32_t inValue) { m_ActiveThreadCount = std::min(inValue, GetThreadCount()); }

	uint32_t GetThreadCount() { return uint32_t(m_Threads.size()); }
	int32_t  GetActiveJobCount() { return m_ActiveJobCount.load(); }

private:
	// per-thread function that waits on and executes tasks
	void ThreadLoop(uint32_t inThreadIndex);

	bool m_Quit = false;
	Mutex m_Mutex;
	Queue<JobPtr> m_JobQueue;
	Array<std::thread> m_Threads;
	Atomic<int32_t> m_ActiveJobCount;
	Atomic<uint8_t> m_ActiveThreadCount;
	std::condition_variable m_ConditionVariable;
};


extern ThreadPool g_ThreadPool;


}