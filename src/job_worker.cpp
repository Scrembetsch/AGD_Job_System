#include "job_worker.h"
#include "../optick/src/optick.h"

#ifdef _WIN32
	#include <Windows.h>
	#include <processthreadsapi.h>
	#ifdef AddJob
		#undef AddJob
	#endif
	#ifdef GetJob
		#undef GetJob
	#endif
#endif

using unique_lock = std::unique_lock<std::mutex>;

std::atomic_uint32_t JobWorker::sWorkerCounter = 0;

JobWorker::JobWorker() : mId(sWorkerCounter++)
{
	mThread = std::thread([this]()
	{
		std::string workerName("Worker " + std::to_string(mId));
		OPTICK_THREAD(workerName.c_str());

		Run();
	});
	SetThreadAffinity();
	// Q: could run thread detached if not wanting them to join?
	//mThread.detach();
}

void JobWorker::SetThreadAffinity()
{
#ifdef _WIN32
	DWORD_PTR dw = SetThreadAffinityMask(mThread.native_handle(), DWORD_PTR(1) << mId);
	if (dw == 0)
	{
		DWORD dwErr = GetLastError();
		HTL_LOGE("SetThreadAffinityMask failed, GLE=" << dwErr << ")");
	}
#endif
}

void JobWorker::AddJob(Job* job)
{
	mJobDeque.PushFront(job);
	mAwakeCondition.notify_one();
}

// HINT: this gets called from within the main thread!
// so either need to synchronize or get rid of
bool JobWorker::AllJobsFinished() const
{
	lock_guard lock(mJobDeque.GetMutex());
	return !(!mJobDeque.IsEmpty() || mJobRunning);
}

size_t JobWorker::GetNumJobs() const
{
	lock_guard lock(mJobDeque.GetMutex());
	return mJobDeque.Size();
}

void JobWorker::Shutdown()
{
	HTL_LOGT(mId, "shutting down worker");
	// breaking the loop (definitely not deathloop reference)
	mRunning = false;

	// need to clear all remaining tasks
	mJobRunning = false;

	lock_guard lock(mJobDeque.GetMutex());
	while (!mJobDeque.IsEmpty()) mJobDeque.PopFront();

	// wait for thread end by waking up and waiting for finish
	mAwakeCondition.notify_one();
	mThread.join();
	HTL_LOGT(mId, "worker thread successfully shutdown");
}

void JobWorker::Run()
{
	HTL_LOGT(mId, "starting worker");
	while (mRunning)
	{
		if (!AnyJobAvailable())
		{
			WaitForJob();
		}
		// Fake job running, so worker doesn't get shut down between getting job and setting JobRunning
		mJobRunning = true;
		if (Job* job = GetJob())
		{
			HTL_LOGT(mId, "starting work on job " << ((job == nullptr) ? "INVALID" : job->GetName()));
			job->Execute();

			// HINT: this can be a problem if setting value gets ordered before job is finished
			// try using a write barrier to ensure val is really written after finish
			//_ReadWriteBarrier(); // TODO: (for MS, std atomic fence)
			// or remove?

			if (job->IsFinished())
			{
				mJobRunning = false;
			}
			else
			{
				HTL_LOGTE(mId, "job not finished after execution :-o");
			}
		}
		else
		{
			// go back to sleep if no executable jobs are available
			HTL_LOGT(mId, "going to sleep on worker thread");
			mJobRunning = false;
			std::this_thread::yield();
		}
	}
	HTL_LOGT(mId, "job end run");
}

bool JobWorker::AnyJobAvailable() const
{
	lock_guard lock(mJobDeque.GetMutex());
	return !mJobDeque.IsEmpty();
}

Job* JobWorker::GetJobFromOwnQueue()
{
	lock_guard lock(mJobDeque.GetMutex());
	// execute our own jobs first
	if (Job* job = mJobDeque.PopFront())
	{
		return job;
	}
	// currently just re-pushing current first element to get the next
	else if (mJobDeque.Size() > 1)
	{
		mJobDeque.PushFront(mJobDeque.PopFront(true));
		if (Job* job = mJobDeque.PopFront())
		{
			return job;
		}
	}
	return nullptr;
}

Job* JobWorker::StealJobFromOtherQueue()
{
	// try stealing from another random worker queue
	unsigned int randomNumber = mRanNumGen.Rand(0, mNumWorkers);
	JobWorker* dequeToStealFrom = &mOtherWorkers[randomNumber];

	// no stealing from ourselves
	if (dequeToStealFrom == this)
	{
		return nullptr;
	}

	lock_guard lock(dequeToStealFrom->mJobDeque.GetMutex());

	if (Job* job = dequeToStealFrom->mJobDeque.PopBack())
	{
		// successfully stolen a job from another queues public end
		HTL_LOGT(mId, "Job successfully stolen");
		return job;
	}
	else
	{
		// no job could be stolen
		return nullptr;
	}

	HTL_LOGT(mId, "no executable job found for queue size: " << mJobDeque.Size() <<
		", job: " << mJobDeque.Front()->GetName() << " with dependencies: " << mJobDeque.Front()->GetUnfinishedJobs());
	return nullptr;
}

Job* JobWorker::GetJob()
{
	Job* job = GetJobFromOwnQueue();
	if (job != nullptr)
	{
		return job;
	}

	return StealJobFromOtherQueue();
}

inline void JobWorker::WaitForJob()
{
	std::unique_lock<std::mutex> lock(mAwakeMutex);
	HTL_LOGT(mId, "waiting for jobs");
	// Awake on JobQueue not empty (work to be done) or Running is disabled (shutdown requested)
	mAwakeCondition.wait(lock, [this]
		{
			lock_guard jobLock(mJobDeque.GetMutex());
			return !mJobDeque.IsEmpty() | !mRunning;
		});
	HTL_LOGT(mId, "awake success!");
}
