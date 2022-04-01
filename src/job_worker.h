#pragma once

#include "job.h"

#include <cstdint>
#include <mutex>
#include <thread>
#include <queue>

class JobWorker
{
private:
	// Use static counter to signal Optick which worker this is
	static std::atomic_uint32_t sWorkerCounter;

public:
	JobWorker();
	~JobWorker();

	bool IsRunning() const;

	void AddJob(Job* job);
	bool AllJobsFinished() const;

private:
	void Run();
	void SetThreadAffinity();

	void WaitForJob();
	Job* GetJob();

	bool mIsRunning;
	uint32_t mId;
	std::thread mThread;
	std::mutex mAwakeMutex;
	std::condition_variable mAwakeCondition;

	// Could also move these two into a container class, but this should be less work for lock-less
	std::mutex mJobQueueMutex;
	std::queue<Job*> mJobQueue;

	std::atomic_bool mJobsTodo;
	std::atomic_bool mJobRunning;
};