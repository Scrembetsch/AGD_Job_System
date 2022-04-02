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
	std::mutex g_i_mutex;
#endif

Job::Job(JobFunc job, std::string name)
	: mJobFunction{ job }, mName{ name }, mParent{ nullptr }
{
}

// allow having one parent to represent dependencies
Job::Job(JobFunc job, std::string name, Job& parent)
	: mJobFunction{ job }, mName{ name }, mParent{ &parent }
{
	mParent->mUnfinishedJobs++;
}

std::string Job::GetName() const
{
	return mName;
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
	Finish();
}

bool Job::IsFinished() const
{
	return (mUnfinishedJobs.load() == 0);
}

void Job::Finish()
{
	//--mUnfinishedJobs; // rmw problem although using atomic value?
	//const int32_t unfinishedJobs = mUnfinishedJobs.fetch_sub(1); // but fetch_ returns previous value :-o
	mUnfinishedJobs.store(mUnfinishedJobs.load() - 1);

#ifdef EXTRA_DEBUG
	std::cout << "job " << mName << " finished with open dependecies: " << mUnfinishedJobs.load() << " on thread #" << std::this_thread::get_id() << "...\n";
#endif

	if (mUnfinishedJobs.load() == 0 && mParent)
	{
		// notify parent that dependent child finished work
		#ifdef EXTRA_DEBUG
			std::cout << "having parent " << (mParent ? mParent->mName : "none") << " with open dependecies: " << (mParent ? (int)mParent->mUnfinishedJobs : 0) << "\n";
		#endif
		mParent->Finish();
	}
}