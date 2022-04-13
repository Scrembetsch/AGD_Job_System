#include "job_system.h"

JobSystem::JobSystem(uint32_t numThreads)
	: mCurrentWorkerId(0)
	, mNumWorkers(numThreads)
	, mWorkers(new JobWorker[numThreads])
{
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		mWorkers[i].mJobSystem = this;
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
	HTL_LOGD("Shutting down jobsystem...");
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		mWorkers[i].Shutdown();
	}
};

// iterate all workers and wake them up if contain resolved dependencies
void JobSystem::WakeThreads()
{
	HTL_LOGD("Trying to wake up threads...");
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		if (mWorkers[i].WakeUp())
		{
			// found the next thread with executable jobs
			return;
		}
	}
	HTL_LOGD("No thread was worthy to wake up... ");
}