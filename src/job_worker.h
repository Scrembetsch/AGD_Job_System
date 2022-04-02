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

class JobWorker
{
private:
	// Use static counter to signal Optick which worker this is
	static std::atomic_uint32_t sWorkerCounter;

public:
	JobWorker();
	~JobWorker();

	void AddJob(Job* job);
	bool AllJobsFinished() const;

	void Shutdown();

private:
	void Run();
	void SetThreadAffinity();

	void WaitForJob();
	Job* GetJob();

	uint32_t mId;
	std::thread mThread;
	std::mutex mAwakeMutex;
	std::condition_variable mAwakeCondition;

#ifdef USING_LOCKLESS
	LocklessQueue mJobQueue;
#else
	LockingQueue mJobQueue;
#endif

	std::atomic_bool mJobRunning = false;
	std::atomic_bool mRunning = true;
};