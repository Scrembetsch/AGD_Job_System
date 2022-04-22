#include "job.h"
#include "defines.h"

Job::Job(JobFunc job, std::string name)
	: mJobFunction{ job }, mName{ name }
{
}

// allow to specify other jobs that define the dependencies
// by only providing dependencies in ctor and not public method,
// we prevent creating circular dependencies
Job::Job(JobFunc job, std::string name, std::vector<Job*> dependants)
	: mJobFunction{ job }, mName{ name }, mDependants{ dependants }
{
	for (auto dependant : mDependants)
	{
		dependant->mUnfinishedJobs++;
		HTL_LOGI("Increment dependency on: " << dependant->mName << " by: " << mName << ", unfinishedJobs: " << dependant->mUnfinishedJobs.load());
	}
}

// need to check if dependencies are met
bool Job::CanExecute() const
{
	return (mUnfinishedJobs.load() == 1);
}

void Job::Execute()
{
	mJobFunction();
	Finish();
}

bool Job::IsFinished() const
{
	// in very rare cases the job correctly updates depending jobs but mUnfinishedJobs is <  0
	return (mUnfinishedJobs.load() <= 0);
}

void Job::Finish()
{
	// atomics override pre and postfix to execute in one instruction
	// https://en.cppreference.com/w/cpp/atomic/atomic/operator_arith
	// otherwise could ran in rmw problems
	// still, between decrementing and reading the state could be changes by another worker
	// so creating and using only a local variable
	int_fast32_t unfinishedJobs = (mUnfinishedJobs.fetch_sub(1) - 1);
	HTL_LOGI("Job " << mName << " finished with open dependecies: " << unfinishedJobs << " on thread #" << std::this_thread::get_id() << "...");

	if (unfinishedJobs == 0 && mDependants.size())
	{
		for (auto dependant : mDependants)
		{
			dependant->mUnfinishedJobs--;
			HTL_LOGI("Having dependant " << dependant->mName << " with now open dependecies: " << (int)dependant->mUnfinishedJobs.load());
		}
	}
}

std::string Job::GetName() const
{
	return mName;
}

int_fast32_t Job::GetUnfinishedJobs() const
{
	return mUnfinishedJobs;
}

bool Job::HasDependants() const
{
	return !mDependants.empty();
}