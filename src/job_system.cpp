#include "job_system.h"

JobSystem::JobSystem(uint32_t numThreads)
	: mCurrentWorkerId(0)
	, mNumWorkers(numThreads)
	, mWorkers(new JobWorker[numThreads])
{
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		mWorkers[i].mNumWorkers = mNumWorkers;
		mWorkers[i].mOtherWorkers = mWorkers;
	}
}

JobSystem::~JobSystem()
{
	delete[] mWorkers;
}

void JobSystem::AddJob(Job* job)
{
	mWorkers[mCurrentWorkerId].AddJob(job);
	// Circle through workers
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

void JobSystem::ShutDown()
{
	HTL_LOGD("shutting down jobsystem...");
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		mWorkers[i].Shutdown();
	}
};