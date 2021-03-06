#include "job_system.h"

JobSystem::JobSystem(uint32_t numThreads)
	: mCurrentWorkerId(0)
	, mNumWorkers(numThreads)
	, mWorkers(new JobWorker[numThreads])
{
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		mWorkers[i].JobSystem = this;
	}
}

JobSystem::~JobSystem()
{
	delete[] mWorkers;
}

void JobSystem::AddJob(Job* job)
{
	mWorkers[mCurrentWorkerId].AddJob(job);
	// circle through workers
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

// iterate all workers and wake them up if dependencies were resolved
void JobSystem::WakeThreads()
{
	HTL_LOGD("Trying to wake up threads...");
	for (uint32_t i = 0; i < mNumWorkers; i++)
	{
		mWorkers[i].WakeUp();
	}
	HTL_LOGD("No thread was worthy to wake up... ");
}

// return a random worker thread id, excluding the one given
unsigned int JobSystem::GetRandomWorkerThreadId(unsigned int threadId)
{
	unsigned int randomNumber = mRanNumGen.Rand(0, mNumWorkers);
	if (threadId == randomNumber)
	{
		// return the following id instead, to prevent stealing from the given (own) thread
		randomNumber = (randomNumber + 1) % mNumWorkers;
	}
	return randomNumber;
}

uint32_t JobSystem::GetNumWorkers() const
{
	return mNumWorkers;
}

JobWorker* JobSystem::GetWorkers()
{
	return mWorkers;
}