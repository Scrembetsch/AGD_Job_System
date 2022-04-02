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
	return !(!mJobDeque.IsEmpty() || mJobRunning);
}

size_t JobWorker::GetNumJobs() const
{
	return mJobDeque.Size();
}

void JobWorker::Shutdown()
{
	HTL_LOGT(mId, "shutting down worker");
	// breaking the loop (definitely not deathloop reference)
	mRunning = false;

	// need to clear all remaining tasks
	mJobRunning = false;
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
		HTL_LOGT(mId, "waiting for job");
		//HTL_LOGD("waiting for job on worker thread #" << std::this_thread::get_id() << "...");

		if (mJobDeque.IsEmpty())
		{
			WaitForJob();
		}
		else if (Job* job = GetJob())
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
			std::this_thread::yield();
		}
	}
	HTL_LOGT(mId, "job end run");
}


Job* JobWorker::GetJob()
{
	if (Job* job = mJobDeque.PopFront())
	{
		mJobRunning = true;
		return job;
	}
	// TODO: try stealing from another worker queue

	// currently just re-pushing current first element to get the next
	else if (mJobDeque.Size() > 1)
	{
		mJobDeque.PushFront(mJobDeque.PopFront(true));
		if (Job* job = mJobDeque.PopFront())
		{
			mJobRunning = true;
			return job;
		}
	}

	HTL_LOGT(mId, "no executable job found for queue size: " << mJobDeque.Size() <<
		", job: " << mJobDeque.Front()->GetName() << " with dependencies: " << mJobDeque.Front()->GetUnfinishedJobs());
	return nullptr;
}

inline void JobWorker::WaitForJob()
{
	std::unique_lock<std::mutex> lock(mAwakeMutex);
	HTL_LOGT(mId, "waiting for jobs");
	// Awake on JobQueue not empty (work to be done) or Running is disabled (shutdown requested)

	mAwakeCondition.wait(lock, [this] { return !mJobDeque.IsEmpty() | !mRunning; });
	HTL_LOGT(mId, "awake success!");
}