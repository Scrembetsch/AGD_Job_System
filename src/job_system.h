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

	unsigned int GetRandomWorkerThreadId(unsigned int threadId);
	uint32_t GetNumWorkers() const;
	JobWorker* GetWorkers();

private:
	uint32_t mCurrentWorkerId;

	// Use basic array instead of vector, because vector complains about deleted copy-constructor
	JobWorker* mWorkers;
	uint32_t mNumWorkers;

	Random mRanNumGen;
};