#pragma once

#include "job_worker.h"

#include <vector>

class JobSystem
{
public:
	JobSystem(uint32_t numThreads);
	~JobSystem();

	// HINT: Probably best to change to Job* when adding dependencies
	void AddJob(Job* job);
	bool AllJobsFinished() const;
private:
	// Q: Should we change this to atomic counter? Shouldn't make too much difference if multiple threads add jobs at the same time
	uint32_t mCurrentWorkerId;

	// Use basic array instead of vector, because vector complains about deleted copy-constructor
	uint32_t mNumWorkers;
	JobWorker* mWorkers;
};