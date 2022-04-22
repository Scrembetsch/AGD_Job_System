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

// use static counter to signal Optick which worker this is
static std::atomic_uint32_t sWorkerCounter{ 0 };

JobWorker::JobWorker() : mId(sWorkerCounter++)
{
	HTL_LOGT(mId, "Creating worker");
	mThread = std::thread([this]()
	{
		std::string workerName("Worker " + std::to_string(mId));
		OPTICK_THREAD(workerName.c_str());

		// setting owning threadId for colored debug output
		mJobDeque.ThreadId = mId;

		Run();
	});
	SetThreadAffinity();
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
	// (*) fix LIFO / FIFO for private / public end
#ifdef HTL_USING_LOCKLESS
	mJobDeque.PushBack(job);
#else
	mJobDeque.PushFront(job);
#endif
	HTL_LOGT(mId, "Pushed " << job->GetName() << " as job #" << mJobDeque.Size() << " to Thread #" << mId);

	{
		std::unique_lock<std::mutex> lock(mAwakeMutex);
		mAwakeCondition.notify_one();
	}
}

bool JobWorker::AllJobsFinished() const
{
	return !(mJobDeque.Size() > 0 || mJobRunning);
}

void JobWorker::Shutdown()
{
	HTL_LOGT(mId, "Shutting down worker");
	// breaking the loop (definitely not deathloop reference)
	mRunning = false;

	// need to clear all remaining tasks
	mJobRunning = false;
	mJobDeque.Clear();

	// wait for thread end by waking up and waiting for finish
	{
		std::unique_lock<std::mutex> lock(mAwakeMutex);
		mAwakeCondition.notify_one();
	}
	mThread.join();
	HTL_LOGT(mId, "Worker thread successfully shutdown");
}

void JobWorker::Run()
{
	HTL_LOGT(mId, "Starting worker");
	while (mRunning)
	{
#ifdef HTL_WAIT_FOR_AVAILABLE_JOBS
		if (mJobDeque.HasExecutableJobs() == false)
		{
			if (JobSystem != nullptr)
			{
				JobSystem->WakeThreads();
			}
			WaitForJob();
		}
#else
		if (mJobDeque.Size() == 0)
		{
			WaitForJob();
		}
#endif

		// fake job running, so worker doesn't get shut down between getting job and setting JobRunning
		mJobRunning = true;
		if (Job* job = GetJob())
		{
			HTL_LOGT(mId, "Starting work on job " << job->GetName());

			job->Execute();
			if (!job->IsFinished())
			{
				HTL_LOGTE(mId, "Job " << job->GetName() << " not finished after execution :-o open unfinishedJobs: " << job->GetUnfinishedJobs());
			}
			mJobRunning = false;
		}
		else
		{
			// yield if no executable jobs are available
			// and hope next worker has available jobs
			HTL_LOGT(mId, "Yield");
			mJobRunning = false;
			std::this_thread::yield();
		}
	}
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

	HTL_LOGT(mId, "No executable job found at all");
	return nullptr;
}

Job* JobWorker::GetJobFromOwnQueue()
{
	// execute our own jobs first
	// private end allow to get unexecutable jobs if has more than one job to be able to reorder
	bool allowOpenDependencies = mJobDeque.Size() > 1;
	if (Job* job = mJobDeque.PopFront(allowOpenDependencies))
	{
		if (job->CanExecute())
		{
			HTL_LOGT(mId, "Job found in current front: " << job->GetName());
			return job;
		}

		// if current front job can't be executed because of dependencies
		// move front to back so we can maybe execute the following job
		// and another worker may steal the dependant one after resolving dependencies
		mJobDeque.PushBack(job);
		if (mJobDeque.Size() > 1)
		{
			HTL_LOGT(mId, "First front job was not executable -> try getting next one from back...");
			if (Job* otherJob = mJobDeque.PopFront())
			{
				HTL_LOGT(mId, "Second job found in current back: " << otherJob->GetName());
				return otherJob;
			}
		}
	}
	HTL_LOGT(mId, "No executable own job found -> check other queues");
	return nullptr;
}

Job* JobWorker::StealJobFromOtherQueue()
{
	if (JobSystem == nullptr || JobSystem->GetNumWorkers() < 2) return nullptr;

	// try stealing from another random worker queue (excluding ourselves)
	// random generated index is the same as internal thread id
	unsigned int randomNumber = JobSystem->GetRandomWorkerThreadId(mId);

	HTL_LOGT(mId, "Try stealing job from worker queue #" << randomNumber);
	JobWorker* workerToStealFrom = &JobSystem->GetWorkers()[randomNumber];
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
	HTL_LOGT(mId, "Waiting for jobs");
	// awake on JobQueue not empty (work to be done) or Running is disabled (shutdown requested)
	std::unique_lock<std::mutex> lock(mAwakeMutex);
	mAwakeCondition.wait(lock, [this]
	{
#ifdef HTL_WAIT_FOR_AVAILABLE_JOBS
		size_t size = mJobDeque.HasExecutableJobs() ? 1 : 0;
#else
		size_t size = mJobDeque.Size();
#endif
		bool running = mRunning;
		HTL_LOGT(mId, "Checking Wake up: Size=" << size << ", Running=" << running << "; Waking up: " << ((size > 0) | !running));

		return ((size > 0) | !running);
	});
	HTL_LOGT(mId, "Awake success!");
}

bool JobWorker::WakeUp()
{
	if (mJobDeque.HasExecutableJobs())
	{
		HTL_LOGT(mId, "Wake up call from job system");
		std::unique_lock<std::mutex> lock(mAwakeMutex);
		mAwakeCondition.notify_one();
		return true;
	}
	return false;
}

void JobWorker::Print() const
{
	HTL_LOG("worker thread " << mId << " running: " << mRunning << ", job running: " << mJobRunning);
	mJobDeque.Print();
}