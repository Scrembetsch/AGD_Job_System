#include "job_system.h"

JobSystem::JobSystem(uint32_t numThreads)
	: mCurrentWorkerId(0)
{
	mWorkers.resize(numThreads);
	//for (uint32_t i = 0; i < numThreads; i++)
	//{
	//	mWorkers.emplace_back();
	//}
}

void JobSystem::AddJob(const Job& job)
{
	mWorkers[mCurrentWorkerId].AddJob(job);
	mCurrentWorkerId = (++mCurrentWorkerId) % mWorkers.size();
}

bool JobSystem::AllJobsFinished() const
{
	for (const auto& worker : mWorkers)
	{
		if (!worker.AllJobsFinished())
		{
			return false;
		}
	}
	return true;
}