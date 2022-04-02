#include "job_worker.h"
#include "../optick/src/optick.h"

#include <string>

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

// TODO: move together with macros from main to defines.h file

using lock_guard = std::lock_guard<std::mutex>;
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

JobWorker::~JobWorker()
{
	// Q: I don't really understand what this should do?
	//mAwakeCondition.notify_one();
	//mThread.join();
	HTL_LOGT(mId, "~destructing worker");
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
	mJobQueue.Push(job);
	mAwakeCondition.notify_one();
}

// HINT: this gets called from within the main thread!
// so either need to synchronize or get rid of
bool JobWorker::AllJobsFinished() const
{
	return !(!mJobQueue.IsEmpty() || mJobRunning);
}

size_t JobWorker::GetNumJobs() const
{
	return mJobQueue.Size();
}

void JobWorker::Shutdown()
{
	HTL_LOGT(mId, "shutdown job...");
	// breaking the worker loop
	mRunning = false;

	// need to clear all remaining tasks
	mJobRunning = false;
	while (!mJobQueue.IsEmpty()) mJobQueue.Pop();

	// wait for thread end
	// Q: either need to wake up to go sleep or just end?
	// A: Without this jobsystem doesn't shutdown correctly
	mAwakeCondition.notify_one();
	mThread.join();
	HTL_LOGT(mId, "job successfully shutdown");
}

void JobWorker::Run()
{
	HTL_LOGT(mId, "starting worker");
	while (mRunning)
	{
		HTL_LOGT(mId, "waiting for job");
		//HTL_LOGD("waiting for job on worker thread #" << std::this_thread::get_id() << "...");

		if (mJobQueue.IsEmpty())
		{
			WaitForJob();
		}
		else if (Job* job = GetJob())
		{
			HTL_LOGD("starting work on job " << ((job == nullptr) ? "INVALID" : job->GetName()) << " on worker thread #" << std::this_thread::get_id() << "...");
			job->Execute();
			//job->Finish(); // EDITED: is now called internally

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
			//HTL__LOGD("going to sleep on worker thread #" << std::this_thread::get_id() << "...\n");

			std::this_thread::yield();
		}
	}
	HTL_LOGT(mId, "job end run");
}

Job* JobWorker::GetJob()
{
	if (Job* job = mJobQueue.Pop())
	{
		mJobRunning = true;
		return job;
	}
	// TODO: try stealing from another worker queue
#ifdef TEST_ONLY_ONE_FRAME
	// currently just re-pushing current first element to get the next
	else if (mJobQueue.Size() > 1)
	{
		mJobQueue.Push(mJobQueue.Pop(true));
		if (Job* job = mJobQueue.Pop())
		{
			mJobRunning = true;
			return job;
		}
	}
#endif

	HTL_LOGD("no executable job found for queue size: " << mJobQueue.Size() << " on worker thread #" << std::this_thread::get_id() << ":");
	return nullptr;
}

inline void JobWorker::WaitForJob()
{
	std::unique_lock<std::mutex> lock(mAwakeMutex);
	HTL_LOGT(mId, "Waiting for jobs...");
	// Awake on JobQueue not empty (work to be done) or Running is disabled (shutdown requested)
	mAwakeCondition.wait(lock, [this] { return !mJobQueue.IsEmpty() | !mRunning; });
	HTL_LOGT(mId, "Awake success");
}