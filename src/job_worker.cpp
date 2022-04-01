#include "job_worker.h"
#include "log.h"
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
}

JobWorker::~JobWorker()
{
	mAwakeCondition.notify_one();
	mThread.join();
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
	lock_guard lock(mJobQueueMutex);
	mJobQueue.push(job);
	mAwakeCondition.notify_one();
}

// HINT: this gets called from within the main thread!
// so either need to synchronize or get rid of
bool JobWorker::AllJobsFinished() const
{
	return !(!mJobQueue.empty() || mJobRunning);
}

void JobWorker::Run()
{
	#ifdef EXTRA_DEBUG
		std::cout << "starting worker thread #" << std::this_thread::get_id() << "...\n";
	#endif
	while (true)
	{
		#ifdef EXTRA_DEBUG
			std::cout << "waiting for job on worker thread #" << std::this_thread::get_id() << "...\n";
		#endif

		if (mJobQueue.empty())
		{
			WaitForJob();
		}
		else if (Job* job = GetJob())
		{
			#ifdef EXTRA_DEBUG
				std::cout << "starting work on job " << ((job == nullptr) ? "INVALID" : job->GetName()) << " on worker thread #" << std::this_thread::get_id() << "...\n";
			#endif
			job->Execute();
			job->Finish();
			
			// HINT: this can be a problem if setting value gets ordered before job is finished
			// try using a write barrier to ensure val is really written after finish
			//_ReadWriteBarrier(); // TODO: (for MS, std atomic fence)
			// or remove?

			if (job->IsFinished())
			{
				mJobRunning = false;
			}
		}
		else
		{
			// go back to sleep if no executable jobs are available
			#ifdef EXTRA_DEBUG
				std::cout << "going to sleep on worker thread #" << std::this_thread::get_id() << "...\n";
			#endif
			std::this_thread::yield();
		}
	}
}

Job* JobWorker::GetJob()
{
	lock_guard lock(mJobQueueMutex);
	if (mJobQueue.empty())
	{
		#ifdef EXTRA_DEBUG
			std::cout << " -> returning because of empty queue from worker thread #" << std::this_thread::get_id() << "...\n";
		#endif
		return nullptr;
	}

	if (mJobQueue.front()->CanExecute())
	{
		mJobRunning = true;
		Job *job = mJobQueue.front();
		mJobQueue.pop();
		return job;
	}
	// TODO: try stealing from another worker queue
	// currently just re-pushing current first element to get the next
	else if (mJobQueue.size() > 1)
	{
		Job* job = mJobQueue.front();
		mJobQueue.pop();
		mJobQueue.push(job);

		if (mJobQueue.front()->CanExecute())
		{
			mJobRunning = true;
			job = mJobQueue.front();
			mJobQueue.pop();
			return job;
		}
	}

	#ifdef EXTRA_DEBUG
		std::cout << "no executable job found for queue size: " << mJobQueue.size() << " on worker thread #" << std::this_thread::get_id() << "...\n";
	#endif
	return nullptr;
}

inline void JobWorker::WaitForJob()
{
	unique_lock lock(mAwakeMutex);
	mAwakeCondition.wait(lock, [this] { return !mJobQueue.empty(); });
}