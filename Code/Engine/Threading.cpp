#include "PCH.h"
#include "Threading.h"

namespace RK {

JobSystem g_JobSystem;

static thread_local bool sIsJobSystemWorker = false;


void Job::Wait() const
{
	uint32_t idle_iterations = 0;

	while (!IsFinished())
	{
		if (g_JobSystem.TryExecuteOne())
		{
			idle_iterations = 0;
			continue;
		}

		if (++idle_iterations < 64)
			std::this_thread::yield();
		else
			std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}


Job::Ptr JobGroup::Schedule(const Job::Function& inFunction, EJobPriority inPriority)
{
	Job::Ptr job = g_JobSystem.Schedule(inFunction, inPriority);
	Add(job);
	return job;
}


Job::Ptr JobGroup::Schedule(const Job::Function& inFunction, Slice<const Job::Ptr> inDependencies, EJobPriority inPriority)
{
	Job::Ptr job = g_JobSystem.Schedule(inFunction, inDependencies, inPriority);
	Add(job);
	return job;
}


void JobGroup::Add(const Job::Ptr& inJob)
{
	std::scoped_lock lock(m_Mutex);
	m_Jobs.push_back(inJob);
}


bool JobGroup::IsFinished() const
{
	std::scoped_lock lock(m_Mutex);

	for (const Job::Ptr& job : m_Jobs)
	{
		if (!job->IsFinished())
			return false;
	}

	return true;
}


void JobGroup::Wait()
{
	for (const Job::Ptr& job : GetJobs())
		job->Wait();

	std::scoped_lock lock(m_Mutex);
	std::erase_if(m_Jobs, [](const Job::Ptr& inJob) { return inJob->IsFinished(); });
}


Array<Job::Ptr> JobGroup::GetJobs() const
{
	std::scoped_lock lock(m_Mutex);
	return m_Jobs;
}


JobSystem::JobSystem() : JobSystem(std::max(std::thread::hardware_concurrency(), 2u) - 1) {}


JobSystem::JobSystem(uint32_t inThreadCount) : m_ActiveThreadCount(inThreadCount)
{
	m_Threads.reserve(inThreadCount);

	for (uint32_t thread_index = 0; thread_index < inThreadCount; thread_index++)
	{
		m_Threads.emplace_back(&JobSystem::WorkerLoop, this, thread_index);

		const WString thread_name = L"Job Thread " + std::to_wstring(thread_index);
		SetThreadDescription((HANDLE)m_Threads.back().native_handle(), thread_name.c_str());
	}
}


JobSystem::~JobSystem()
{
	Shutdown();
}


Job::Ptr JobSystem::Schedule(const Job::Function& inFunction, EJobPriority inPriority)
{
	return Schedule(inFunction, {}, inPriority);
}


Job::Ptr JobSystem::Schedule(const Job::Function& inFunction, Slice<const Job::Ptr> inDependencies, EJobPriority inPriority)
{
	Job::Ptr job = std::make_shared<Job>(inFunction, inPriority);

	m_ActiveJobCount.fetch_add(1);
	job->m_PendingDependencies.store(1);

	for (const Job::Ptr& dependency : inDependencies)
	{
		if (!dependency)
			continue;

		std::scoped_lock lock(dependency->m_DependentsMutex);

		if (!dependency->IsFinished())
		{
			job->m_PendingDependencies.fetch_add(1);
			dependency->m_Dependents.push_back(job);
		}
	}

	if (job->m_PendingDependencies.fetch_sub(1) == 1)
		Enqueue(job);

	return job;
}


void JobSystem::ParallelFor(uint32_t inCount, uint32_t inBatchSize, const std::function<void(uint32_t inIndex)>& inFunction)
{
	if (inCount == 0)
		return;

	const uint32_t batch_size = std::max(inBatchSize, 1u);
	const uint32_t batch_count = ( inCount + batch_size - 1 ) / batch_size;

	Atomic<uint32_t> next_batch = 0;

	auto RunBatches = [&]()
	{
		for (uint32_t batch = next_batch.fetch_add(1); batch < batch_count; batch = next_batch.fetch_add(1))
		{
			const uint32_t begin = batch * batch_size;
			const uint32_t end = std::min(begin + batch_size, inCount);

			for (uint32_t index = begin; index < end; index++)
				inFunction(index);
		}
	};

	JobGroup helpers;

	const uint32_t helper_count = std::min(batch_count - 1, GetThreadCount());

	for (uint32_t helper_index = 0; helper_index < helper_count; helper_index++)
		helpers.Schedule(RunBatches, JOB_PRIORITY_HIGH);

	try
	{
		RunBatches();
	}
	catch (...)
	{
		next_batch.store(batch_count);
		helpers.Wait();
		throw;
	}

	helpers.Wait();
}


bool JobSystem::TryExecuteOne()
{
	Job::Ptr job = nullptr;

	{
		std::scoped_lock lock(m_QueueMutex);
		job = PopJob();
	}

	if (!job)
		return false;

	Execute(job);
	return true;
}


void JobSystem::WaitForAll()
{
	assert(!sIsWorkerThread() && "WaitForAll would wait on the job it is called from");

	while (m_ActiveJobCount.load() > 0)
	{
		if (TryExecuteOne())
			continue;

		std::unique_lock lock(m_QueueMutex);
		m_IdleCondition.wait_for(lock, std::chrono::milliseconds(1), [this]() { return m_ActiveJobCount.load() == 0; });
	}
}


void JobSystem::Shutdown()
{
	{
		std::scoped_lock lock(m_QueueMutex);

		if (m_Quit)
			return;

		m_Quit = true;
		m_ActiveThreadCount.store(GetThreadCount());
	}

	m_QueueCondition.notify_all();

	for (std::thread& thread : m_Threads)
	{
		if (thread.joinable())
			thread.join();
	}
}


void JobSystem::SetActiveThreadCount(uint32_t inValue)
{
	{
		std::scoped_lock lock(m_QueueMutex);
		m_ActiveThreadCount.store(std::clamp(inValue, 1u, std::max(GetThreadCount(), 1u)));
	}

	m_QueueCondition.notify_all();
}


bool JobSystem::sIsWorkerThread()
{
	return sIsJobSystemWorker;
}


void JobSystem::Enqueue(const Job::Ptr& inJob)
{
	{
		std::scoped_lock lock(m_QueueMutex);
		m_Queues[inJob->m_Priority].push_back(inJob);
	}

	m_QueueCondition.notify_one();
}


void JobSystem::Execute(const Job::Ptr& inJob)
{
	try
	{
		inJob->m_Function();
	}
	catch (const std::exception& inException)
	{
		gLogError("Jobs", "Job threw an exception: {}", inException.what());
	}
	catch (...)
	{
		gLogError("Jobs", "Job threw an unknown exception");
	}

	inJob->m_Function = nullptr;

	Array<Job::Ptr> dependents;

	{
		std::scoped_lock lock(inJob->m_DependentsMutex);
		inJob->m_Finished.store(true, std::memory_order_release);
		dependents.swap(inJob->m_Dependents);
	}

	for (const Job::Ptr& dependent : dependents)
	{
		if (dependent->m_PendingDependencies.fetch_sub(1) == 1)
			Enqueue(dependent);
	}

	if (m_ActiveJobCount.fetch_sub(1) == 1)
	{
		std::scoped_lock lock(m_QueueMutex);
		m_IdleCondition.notify_all();
	}
}


Job::Ptr JobSystem::PopJob()
{
	for (std::deque<Job::Ptr>& queue : m_Queues)
	{
		if (!queue.empty())
		{
			Job::Ptr job = std::move(queue.front());
			queue.pop_front();
			return job;
		}
	}

	return nullptr;
}


bool JobSystem::HasQueuedJobs() const
{
	for (const std::deque<Job::Ptr>& queue : m_Queues)
	{
		if (!queue.empty())
			return true;
	}

	return false;
}


void JobSystem::WorkerLoop(uint32_t inThreadIndex)
{
	sIsJobSystemWorker = true;

	while (true)
	{
		Job::Ptr job = nullptr;

		{
			std::unique_lock lock(m_QueueMutex);

			m_QueueCondition.wait(lock, [this, inThreadIndex]()
			{
				return m_Quit || ( HasQueuedJobs() && inThreadIndex < m_ActiveThreadCount.load() );
			});

			if (m_Quit && !HasQueuedJobs())
				return;

			job = PopJob();
		}

		if (job)
			Execute(job);
	}
}

}
