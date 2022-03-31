#include "job_system.h"

JobSystem::JobSystem(uint32_t numThreads)
	: mCurrentWorkerId(0)
	, mNumWorkers(numThreads)
	, mWorkers(new JobWorker[numThreads])
{
}

JobSystem::~JobSystem()
{
	delete[] mWorkers;
}

void JobSystem::AddJob(Job* job)
{
	mWorkers[mCurrentWorkerId].AddJob(job);
	mCurrentWorkerId = (++mCurrentWorkerId) % mNumWorkers;
}

bool JobSystem::AllJobsFinished() const
{
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		if (!mWorkers[i].AllJobsFinished())
		{
			return false;
		}
	}
	return true;
}