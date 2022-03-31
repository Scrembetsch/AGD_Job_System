#include "job_worker.h"

using lock_guard = std::lock_guard<std::mutex>;
using unique_lock = std::unique_lock<std::mutex>;

JobWorker::JobWorker()
	: mIsRunning(true)
	, mJobsTodo(false)
{
	mThread = std::thread([this]() { Run(); });
}

JobWorker::~JobWorker()
{
}

JobWorker::JobWorker(const JobWorker& other)
	: mIsRunning(other.mIsRunning)
{
	mThread = std::thread([this]() { Run(); });
}

void JobWorker::SetIsRunning(bool running)
{
	mIsRunning = running;
	mAwakeCondition.notify_one();
}

bool JobWorker::IsRunning() const
{
	return mIsRunning;
}

void JobWorker::AddJob(const Job& job)
{
	lock_guard lock(mJobQueueMutex);
	mJobQueue.push(job);
	mJobsTodo = true;
	mAwakeCondition.notify_one();
}

bool JobWorker::AllJobsFinished() const
{
	return !(mJobsTodo || mJobRunning);
}

void JobWorker::Run()
{
	while (mIsRunning)
	{
		if (mJobQueue.empty())
		{
			WaitForJob();
		}
		else
		{
			Job job;
			if (GetJob(job))
			{
				job.JobFunction();
				mJobRunning = false;
			}
		}
	}
}

bool JobWorker::GetJob(Job& job)
{
	if (mJobQueue.empty())
	{
		return false;
	}

	lock_guard lock(mJobQueueMutex);
	if (CanExecute(mJobQueue.front()))
	{
		job = mJobQueue.front();
		mJobQueue.pop();

		if (mJobQueue.empty())
		{
			mJobsTodo = false;
		}
		mJobRunning = true;
		return true;
	}
	return false;
}

bool JobWorker::CanExecute(const Job& job) const
{
	// TODO: Add dependencies
	return true;
}

void JobWorker::WaitForJob()
{
	unique_lock lock(mAwakeMutex);
	mAwakeCondition.wait(lock, [this] { return !mJobQueue.empty(); });
}