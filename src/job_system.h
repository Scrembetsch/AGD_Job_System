#pragma once

#include "job_worker.h"
#include "random.h"

class JobSystem
{
public:
	JobSystem(uint32_t numThreads);
	~JobSystem();

	void AddJob(Job* job);
	bool AllJobsFinished() const;

	void ShutDown();
	void WakeThreads();

	Random mRanNumGen;

	// Use basic array instead of vector, because vector complains about deleted copy-constructor
	uint32_t mNumWorkers;
	JobWorker* mWorkers;

private:
	// Q: Should we change this to atomic counter? Shouldn't make too much difference if multiple threads add jobs at the same time
	// LEM: I think we are only add jobs from main_runner thread so this shouldn't be necessary
	uint32_t mCurrentWorkerId;
};