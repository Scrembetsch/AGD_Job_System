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
//#define EXTRA_DEBUG
#ifdef EXTRA_DEBUG
	#include <iostream>
#endif

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
#ifdef EXTRA_DEBUG
	thread_log(mId, "~destructing worker");
#endif
}

void JobWorker::SetThreadAffinity()
{
#ifdef _WIN32
	DWORD_PTR dw = SetThreadAffinityMask(mThread.native_handle(), DWORD_PTR(1) << mId);
	if (dw == 0)
	{
		DWORD dwErr = GetLastError();
		HTL_LOGE("SetThreadAffinityMask failed, GLE=%lu", dwErr);
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

void JobWorker::Shutdown()
{
#ifdef EXTRA_DEBUG
	thread_log(mId, "shutdown job...");
#endif
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
#ifdef EXTRA_DEBUG
	thread_log(mId, "job successfully shutdown");
#endif
}

void JobWorker::Run()
{
	#ifdef EXTRA_DEBUG
		thread_log(mId, "starting worker");
	#endif
	while (mRunning)
	{
		#ifdef EXTRA_DEBUG
			thread_log(mId, "waiting for job");
			//std::cout << "waiting for job on worker thread #" << std::this_thread::get_id() << "...\n";
		#endif

		if (mJobQueue.IsEmpty())
		{
			WaitForJob();
		}
		else if (Job* job = GetJob())
		{
			#ifdef EXTRA_DEBUG
				std::cout << "starting work on job " << ((job == nullptr) ? "INVALID" : job->GetName()) << " on worker thread #" << std::this_thread::get_id() << "...\n";
			#endif
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
				thread_log(1, "\t!!! ERROR: job not finished after execution :-o");
			}
		}
		else
		{
			// go back to sleep if no executable jobs are available
			#ifdef EXTRA_DEBUG
				thread_log(mId, "going to sleep on worker thread");
				//std::cout << "going to sleep on worker thread #" << std::this_thread::get_id() << "...\n";
			#endif

			std::this_thread::yield();
		}
	}
	#ifdef EXTRA_DEBUG
		thread_log(mId, "job end run");
	#endif
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

	#ifdef EXTRA_DEBUG
		std::cout << "no executable job found for queue size: " << mJobQueue.Size() << " on worker thread #" << std::this_thread::get_id() << ":\n";
	#endif
	return nullptr;
}

inline void JobWorker::WaitForJob()
{
	unique_lock lock(mAwakeMutex);
	thread_log(mId, "awaking");
	// Awake on JobQueue not empty (work to be done) or Running is disabled (shutdown requested)
	mAwakeCondition.wait(lock, [this] { return !mJobQueue.IsEmpty() | !mRunning; });
}