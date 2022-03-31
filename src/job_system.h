#pragma once

#include "job_worker.h"

#include <vector>

class JobSystem
{
public:
	JobSystem(uint32_t numThreads);
	~JobSystem() = default;

	// Copy job or allocate on heap and pass pointer?
	void AddJob(const Job& job);
	bool AllJobsFinished() const;
private:
	// Not atomic because it shouldn't be too bad if multiple threads add jobs at the same time
	uint32_t mCurrentWorkerId;
	std::vector<JobWorker> mWorkers;
};