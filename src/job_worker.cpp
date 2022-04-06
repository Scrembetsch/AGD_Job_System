#include "job_worker.h"
#include "job_system.h"
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
	HTL_LOGT(mId, "creating worker");
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
#else
	HTL_LOGW("SetThreadAffinityMask currently only supported on _WIN32");
#endif
}

void JobWorker::AddJob(Job* job)
{
	mJobDeque.PushFront(job);
	mAwakeCondition.notify_one();
	HTL_LOGI("Pushed job to Thread #" << mId);
}

bool JobWorker::AllJobsFinished() const
{
	// HINT: this gets called from within the main thread!
	// so either need to synchronize mJobRunning or get rid of
	return !(!mJobDeque.IsEmpty() || mJobRunning);
}

void JobWorker::Shutdown()
{
	HTL_LOGT(mId, "shutting down worker");
	// breaking the loop (definitely not deathloop reference)
	mRunning = false;

	// need to clear all remaining tasks
	mJobRunning = false;

	while (mJobDeque.PopFront());

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
			//_ReadWriteBarrier(); // for MS, std atomic fence from c++ standard

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
			// yield if no executable jobs are available
			HTL_LOGT(mId, "yielding on worker thread");
			mJobRunning = false;
			std::this_thread::yield();
		}
	}
	HTL_LOGT(mId, "job end run");
}

bool JobWorker::AnyJobAvailable() const
{
	return !mJobDeque.IsEmpty();
}

Job* JobWorker::GetJob()
{
	if (Job* job = GetJobFromOwnQueue())
	{
		return job;
	}
	else if (Job* job = StealJobFromOtherQueue())
	{
		return job;
	}

	HTL_LOGT(mId, "no executable job found for queue size: " << mJobDeque.Size());
	return nullptr;
}

Job* JobWorker::GetJobFromOwnQueue()
{
	// execute our own jobs first
	if (Job* job = mJobDeque.PopFront())
	{
		HTL_LOGT(mId, "job found in current front: " << job->GetName());
		return job;
	}
	// if current front job can't be executed because of dependencies
	// move front to back so we can maybe execute the following job
	// and another worker may steal the dependant one
	else if (Job* job = mJobDeque.HireBack())
	{
		HTL_LOGT(mId, "job found in hire back: " << job->GetName());
		return job;
	}
	return nullptr;
}

Job* JobWorker::StealJobFromOtherQueue()
{
	if (mJobSystem->mNumWorkers < 2) return nullptr;

	// try stealing from another random worker queue
	unsigned int randomNumber = mJobSystem->mRanNumGen.Rand(0, mJobSystem->mNumWorkers);
	// no stealing from ourselves, try the next one instead
	// random generated index is the same as internal thread id
	if (randomNumber == mId)
	{
		randomNumber = (randomNumber + 1) % mJobSystem->mNumWorkers;
	}

	HTL_LOGT(mId, "Stealing job from worker queue #" << randomNumber);
	JobWorker* workerToStealFrom = &mJobSystem->mWorkers[randomNumber];
	if (Job* job = workerToStealFrom->mJobDeque.PopBack())
	{
		// successfully stolen a job from another queues public end
		HTL_LOGT(mId, "Job " << job->GetName() << " successfully stolen");
		return job;
	}
	return nullptr;
}

inline void JobWorker::WaitForJob()
{
	HTL_LOGT(mId, "waiting for jobs");
	// Awake on JobQueue not empty (work to be done) or Running is disabled (shutdown requested)
	std::unique_lock<std::mutex> lock(mAwakeMutex);
	mAwakeCondition.wait(lock, [this]
	{
		size_t size = mJobDeque.Size();
		bool running = mRunning;
		HTL_LOGT(mId, "Checking Wake up: Size=" << size << ", Running=" << mRunning << "; Waking up: " << (!mJobDeque.IsEmpty() | !mRunning));

		return size > 0 | !running;
	});
	HTL_LOGT(mId, "awake success!");
}
