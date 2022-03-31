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

JobWorker::JobWorker()
	: mIsRunning(true)
	, mJobsTodo(false)
	, mJobRunning(false)
	, mId(sWorkerCounter++)
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
	mIsRunning = false;
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

bool JobWorker::IsRunning() const
{
	return mIsRunning;
}

void JobWorker::AddJob(Job* job)
{
	lock_guard lock(mJobQueueMutex);
	mJobQueue.push(job);
	mJobsTodo = true;
	mAwakeCondition.notify_one();
}

bool JobWorker::AllJobsFinished() const
{
	return !(mJobsTodo | mJobRunning);
}

void JobWorker::Run()
{
	#ifdef EXTRA_DEBUG
		std::cout << "starting worker thread #" << std::this_thread::get_id() << "...\n";
	#endif
	while (mIsRunning)
	{
		#ifdef EXTRA_DEBUG
			std::cout << "checking for open jobs on worker thread #" << std::this_thread::get_id() << "...\n";
		#endif
		if (mJobQueue.empty())
		{
			#ifdef EXTRA_DEBUG
				std::cout << "waiting for job on worker thread #" << std::this_thread::get_id() << "...\n";
			#endif
			WaitForJob();
		}
		else
		{
			// TODO: return Job * instead of bool
			//if (Job* job = GetJob())
			Job* job = nullptr;
			if (GetJob(&job))
			{
				job->Execute();
				job->Finish();
				mJobRunning = false;
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
}

// TODO: return Job * instead of bool
bool JobWorker::GetJob(Job** job)
{
	if (mJobQueue.empty())
	{
		return false;
	}

	lock_guard lock(mJobQueueMutex);
	if (mJobQueue.front()->CanExecute())
	{
		*job = mJobQueue.front();
		mJobQueue.pop();

		// JobRunning HAS to be set before mJobsTodo, otherwise job-system could die
		mJobRunning = true;
		if (mJobQueue.empty())
		{
			mJobsTodo = false;
		}
		return true;
	}
	// TODO: try stealing from another worker queue
	// currently just re-pushing current first element to get the next
	else if (mJobQueue.size() > 1)
	{
		*job = mJobQueue.front();
		mJobQueue.pop();
		mJobQueue.push(*job);

		if (mJobQueue.front()->CanExecute())
		{
			*job = mJobQueue.front();
			mJobQueue.pop();

			// JobRunning HAS to be set before mJobsTodo, otherwise job-system could die
			mJobRunning = true;
			if (mJobQueue.empty())
			{
				mJobsTodo = false;
			}
			return true;
		}
	}
	return false;
}

void JobWorker::WaitForJob()
{
	unique_lock lock(mAwakeMutex);
	mAwakeCondition.wait(lock, [this] { return !mJobQueue.empty(); });
}