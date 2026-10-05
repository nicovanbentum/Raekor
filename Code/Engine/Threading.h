#pragma once

namespace RK {

enum EJobPriority
{
	JOB_PRIORITY_HIGH,
	JOB_PRIORITY_NORMAL,
	JOB_PRIORITY_LOW,
	JOB_PRIORITY_COUNT
};


class Job
{
public:
	friend class JobSystem;

	using Ptr = SharedPtr<Job>;
	using Function = std::function<void()>;

	Job(const Function& inFunction, EJobPriority inPriority) : m_Function(inFunction), m_Priority(inPriority) {}

	bool IsFinished() const { return m_Finished.load(std::memory_order_acquire); }

	void Wait() const;

private:
	Function m_Function;
	EJobPriority m_Priority;
	Atomic<bool> m_Finished = false;
	Atomic<uint32_t> m_PendingDependencies = 0;

	Mutex m_DependentsMutex;
	Array<Ptr> m_Dependents;
};


class JobGroup
{
public:
	JobGroup() = default;
	~JobGroup() { Wait(); }

	JobGroup(const JobGroup&) = delete;
	JobGroup& operator=(const JobGroup&) = delete;

	Job::Ptr Schedule(const Job::Function& inFunction, EJobPriority inPriority = JOB_PRIORITY_NORMAL);
	Job::Ptr Schedule(const Job::Function& inFunction, Slice<const Job::Ptr> inDependencies, EJobPriority inPriority = JOB_PRIORITY_NORMAL);

	void Add(const Job::Ptr& inJob);

	bool IsFinished() const;
	void Wait();

	Array<Job::Ptr> GetJobs() const;

private:
	mutable Mutex m_Mutex;
	Array<Job::Ptr> m_Jobs;
};


class JobSystem
{
public:
	JobSystem();
	explicit JobSystem(uint32_t inThreadCount);
	~JobSystem();

	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;

	Job::Ptr Schedule(const Job::Function& inFunction, EJobPriority inPriority = JOB_PRIORITY_NORMAL);
	Job::Ptr Schedule(const Job::Function& inFunction, Slice<const Job::Ptr> inDependencies, EJobPriority inPriority = JOB_PRIORITY_NORMAL);

	void ParallelFor(uint32_t inCount, uint32_t inBatchSize, const std::function<void(uint32_t inIndex)>& inFunction);

	bool TryExecuteOne();

	void WaitForAll();

	void Shutdown();

	void SetActiveThreadCount(uint32_t inValue);

	uint32_t GetThreadCount() const { return uint32_t(m_Threads.size()); }
	int32_t GetActiveJobCount() const { return m_ActiveJobCount.load(); }

	static bool sIsWorkerThread();

private:
	void Enqueue(const Job::Ptr& inJob);
	void Execute(const Job::Ptr& inJob);
	Job::Ptr PopJob();
	bool HasQueuedJobs() const;
	void WorkerLoop(uint32_t inThreadIndex);

	bool m_Quit = false;
	Atomic<int32_t> m_ActiveJobCount = 0;
	Atomic<uint32_t> m_ActiveThreadCount = 0;

	Mutex m_QueueMutex;
	std::condition_variable m_QueueCondition;
	std::condition_variable m_IdleCondition;
	StaticArray<std::deque<Job::Ptr>, JOB_PRIORITY_COUNT> m_Queues;

	Array<std::thread> m_Threads;
};


extern JobSystem g_JobSystem;

}
