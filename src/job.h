#pragma once

#include <atomic>
#include <thread>

// TODO: remove/comment all stuff for printing information
// setting up a guard_lock to be able to sync print and using cout for printing thread_id
#include <string>

class Job
{
private:
	typedef void (*JobFunc)();
	JobFunc mJobFunction;

	// TODO: in the first iteration we are using just one parent to define dependencies
	// later a list of dependencies will be implemented allowing more constraints
	Job* mParent;

	// atomic type to ensure inc and dec are visible to all threads
	// 0 means jobs done
	// 1 means "this" job has not finished yet
	// value > 1 means having open dependencies
	std::atomic_int_fast32_t mUnfinishedJobs{ 1 };

	// may use data and context of any kind but not supported in this exercise
	//void* mData;
	//void* mContext;

	// additional debug information by providing a readable task name
	std::string mName;

public:
	// we don't support jobs with not worker function
	Job() = delete;

	// we also don't want Jobs to be copied because it doesn't make much sense
	// but also because handling internal atomics don't like this
	Job(const Job&) = delete;
	Job& operator =(const Job&) = delete;

	// TODO: what about move semantics?

	Job(JobFunc job, std::string name);

	// allow having one parent to represent dependencies
	Job(JobFunc job, std::string name, Job& parent);

	std::string GetName() const;

	// need something to check if dependencies are met
	// TODO: are there any other constraints so the Job can't be started?
	bool CanExecute() const;

	void Execute();

	bool IsFinished() const;

	void Finish();
};