#pragma once

#include "job.h"
#include "defines.h"

// TODO: remove/comment all stuff for printing information
// setting up a guard_lock to be able to sync print and using cout for printing thread_id
#include <iostream> // using cout to print thread_id, otherwise would need to create a stringstream
#include <mutex>

#ifdef EXTRA_DEBUG
	// TODO: remove/comment all stuff for printing information
	// setting up a guard_lock to be able to sync print and using cout for printing thread_id
#endif

Job::Job(JobFunc job, std::string name)
	: mJobFunction{ job }, mName{ name }, mParent{ nullptr }
{
}

// allow having one parent to represent dependencies
Job::Job(JobFunc job, std::string name, Job* parent)
	: mJobFunction{ job }, mName{ name }, mParent{ parent }
{
	mParent->mUnfinishedJobs++;
	HTL_LOGI("Increment on " << mParent->mName << " by: " << mName << ", Now: " << mParent->mUnfinishedJobs.load());
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

// need something to check if dependencies are met
// TODO: are there any other constraints so the Job can't be started?
bool Job::CanExecute() const
{
	return (mUnfinishedJobs == 1);
}

void Job::Execute()
{
	mJobFunction();
	Finish(false);
}

bool Job::IsFinished() const
{
	return (mUnfinishedJobs.load() == 0);
}

void Job::Finish(bool finishFromChild)
{
	//--mUnfinishedJobs; // rmw problem although using atomic value?
	//const int32_t unfinishedJobs = mUnfinishedJobs.fetch_sub(1); // but fetch_ returns previous value :-o
	mUnfinishedJobs--;
	HTL_LOGI("Decrement on " << mName << ", Now: " << mUnfinishedJobs.load());
	HTL_LOGI("job " << mName << " finished with open dependecies: " << mUnfinishedJobs.load() << " on thread #" << std::this_thread::get_id() << "...");

	// Finish from child needed because:
	// This block may be executed after the job itself is already finished -> decrements recursively instead of only one parent
	// Example: Physics -> Collision -> Rendering
	// Physics is finished and decrements collision -> collision dependency = 1 (would not jump to next parent)
	// Before next block is executed, collision is finished by other worker -> dependencies = 0
	// This results in the next block being executed 2 times -> Rendering dependencies counting into negative
	if (mUnfinishedJobs.load() == 0
		&& mParent
		&& !finishFromChild)
	{
		// notify parent that dependent child finished work
		HTL_LOGI("having parent " << (mParent ? mParent->mName : "none") << " with open dependecies: " << (mParent ? (int)mParent->mUnfinishedJobs : 0));
		mParent->Finish(true);
	}
}