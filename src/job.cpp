#include "job.h"
#include "defines.h"

Job::Job(JobFunc job, std::string name)
	: mJobFunction{ job }, mName{ name }, mParent{ nullptr }
{
}

// allow having one parent to represent dependencies
Job::Job(JobFunc job, std::string name, Job* parent)
	: mJobFunction{ job }, mName{ name }, mParent{ parent }
{
	mParent->mUnfinishedJobs++;
	HTL_LOGI("Increment dependency on: " << mParent->mName << " by: " << mName << ", unfinishedJobs: " << mParent->mUnfinishedJobs.load());
}

std::string Job::GetName() const
{
	return mName;
}

void Job::AddDependency()
{
	mUnfinishedJobs++;
	//if (mParent != nullptr)
	//{
	//	mParent->AddDependency();
	//}
}

// need to check if dependencies are met
// TODO: are there any additional other constraints so the Job can't be started?
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
	return (mUnfinishedJobs.load() == 0);
}

void Job::Finish()
{
	// atomics override pre and postfix to execute in one instruction
	// https://en.cppreference.com/w/cpp/atomic/atomic/operator_arith
	// otherwise could ran in rmw problems
	// still, between decrementing and reading the state could be changes by another worker
	// so creating and using only a local variable
	std::int_fast32_t unfinishedJobs = (mUnfinishedJobs.fetch_sub(1) - 1);
	HTL_LOGI("job " << mName << " finished with open dependecies: " << unfinishedJobs <<
		" having parent: " << (mParent ? mParent->mName : "none") <<
		" on thread #" << std::this_thread::get_id() << "...");

	if (unfinishedJobs == 0 && mParent)
	{
		mParent->mUnfinishedJobs--;
		HTL_LOGI("having parent " << (mParent ? mParent->mName : "none") << " with now open dependecies: " << (mParent ? (int)mParent->mUnfinishedJobs.load() : 0));
	}
}