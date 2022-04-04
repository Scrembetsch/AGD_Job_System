#pragma once

#include "defines.h"
#ifdef USING_LOCKLESS
	#include "lockless_queue.h"
#else
	#include "locking_queue.h"
#endif

#include <cstdint>
#include <mutex>
#include <thread>
class JobSystem;

class JobWorker
{
private:
	// Use static counter to signal Optick which worker this is
	static std::atomic_uint32_t sWorkerCounter;

	using lock_guard = std::lock_guard<std::mutex>;
public:
	JobWorker();

	void AddJob(Job* job);
	bool AllJobsFinished() const;

	void Shutdown();

	JobSystem* mJobSystem;

private:
	void Run();
	void SetThreadAffinity();

	void WaitForJob();
	Job* GetJob();
	Job* GetJobFromOwnQueue();
	Job* StealJobFromOtherQueue();

	bool AnyJobAvailable() const;

	uint32_t mId;
	std::thread mThread;
	std::mutex mAwakeMutex;
	std::condition_variable mAwakeCondition;

#ifdef USING_LOCKLESS
	LocklessQueue mJobQueue;
#else
	LockingDeque mJobDeque;
#endif

	std::atomic_bool mJobRunning = false;
	std::atomic_bool mRunning = true;
};