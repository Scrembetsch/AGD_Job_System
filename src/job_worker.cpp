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
		std::string workerName("Worker ");
		workerName += std::to_string(mId);
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

void JobWorker::AddJob(const Job& job)
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
	while (mIsRunning)
	{
		if (mJobQueue.empty())
		{
			WaitForJob();
		}
		else
		{
			Job job(nullptr);
			if (GetJob(job))
			{
				job.JobFunction();
				mJobRunning = false;
			}
			else
			{
				std::this_thread::yield();
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