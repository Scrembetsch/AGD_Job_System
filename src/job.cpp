#pragma once

#include "job.h"

// TODO: remove/comment all stuff for printing information
// setting up a guard_lock to be able to sync print and using cout for printing thread_id
#include <iostream> // using cout to print thread_id, otherwise would need to create a stringstream
#include <mutex>

// TODO: move together with macros from other files to defines.h file
//#define EXTRA_DEBUG // allows locked print output
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

// TODO: check if actually need copying
// but if using an atomic member variable is can't be just copied and musst be loaded
Job::Job(const Job& other)
	: mJobFunction{ other.mJobFunction }, mParent{ other.mParent }, mName{ other.mName }, mUnfinishedJobs(other.mUnfinishedJobs.load(std::memory_order_seq_cst))
{
}

Job& Job::operator = (const Job& other)
{
	if (this != &other) // not a self-assignment
	{
		mJobFunction = other.mJobFunction;
		mParent = other.mParent;
		mName = other.mName;
		mUnfinishedJobs = other.mUnfinishedJobs.load(std::memory_order_seq_cst);
	}
	return *this;
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
}

bool Job::IsFinished() const
{
	return (mUnfinishedJobs == 0);
}

void Job::Finish()
{
	mUnfinishedJobs--;

#ifdef EXTRA_DEBUG
	const std::lock_guard<std::mutex> lock(g_i_mutex);
	std::cout << "job " << mName << " finished with open dependecies: " << mUnfinishedJobs << " on thread #" << std::this_thread::get_id() << "...\n";
#endif

	if (IsFinished())
	{
		// notify parent that dependent child finished work
		#ifdef EXTRA_DEBUG
			std::cout << "having parent " << (mParent ? mParent->mName : "none") << " with open dependecies: " << (mParent ? (int)mParent->mUnfinishedJobs : 0) << "\n";
		#endif
		if (mParent != nullptr)
		{
			mParent->Finish();
		}
	}
}