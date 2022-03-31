#pragma once

#include "job.h"

#include <cstdint>
#include <mutex>
#include <thread>
#include <queue>

class JobWorker
{
public:
	JobWorker();
	~JobWorker();
	JobWorker(const JobWorker& other);

	void SetIsRunning(bool running);
	bool IsRunning() const;

	void AddJob(const Job& job);
	bool AllJobsFinished() const;
private:
	void Run();

	void WaitForJob();
	bool GetJob(Job& job);
	bool CanExecute(const Job& job) const;

	bool mIsRunning;
	std::thread mThread;
	std::mutex mAwakeMutex;
	std::condition_variable mAwakeCondition;

	// Could also move these to in a container class, but this should be less work for lock-less
	std::mutex mJobQueueMutex;
	std::queue<Job> mJobQueue;
	std::atomic_bool mJobsTodo;
	std::atomic_bool mJobRunning;
};