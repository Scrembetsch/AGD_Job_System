#pragma once

#include "defines.h"
#ifdef USING_LOCKLESS
	#include "lockless_deque.h"
#else
	#include "locking_deque.h"
#endif

class JobSystem;

class JobWorker
{
private:
	uint32_t mId;
	std::thread mThread;
	std::mutex mAwakeMutex;
	std::condition_variable mAwakeCondition;

#ifdef USING_LOCKLESS
	LocklessDeque mJobDeque;
#else
	LockingDeque mJobDeque;
#endif

	std::atomic_bool mJobRunning{ false };
	std::atomic_bool mRunning{ true };

	void Run();
	void SetThreadAffinity();

	void WaitForJob();
	Job* GetJob();
	Job* GetJobFromOwnQueue();
	Job* StealJobFromOtherQueue();

public:
	JobWorker();

	void AddJob(Job* job);
	bool AllJobsFinished() const;

	void Shutdown();

	JobSystem* mJobSystem;

	void Print()
	{
		HTL_LOG("worker thread " << mId << " running: " << mRunning << ", job running: " << mJobRunning);
		mJobDeque.Print();
	}
};