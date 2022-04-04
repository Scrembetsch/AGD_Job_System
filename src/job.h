#pragma once

#include <atomic>
#include <thread>
#include <vector>

// TODO: remove/comment all stuff for printing information
#include <string>

class Job
{
private:
	typedef void (*JobFunc)();
	JobFunc mJobFunction;

	// first iteration: using just one parent to define dependencies
	//Job* mParent;
	// a list of dependants, allowing more constraints
	std::vector<Job*> mDependants;

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

	// also deleting move semantics otherwise would need to update dependeny pointers
	Job(Job&&) = delete;
	Job& operator=(Job&&) = delete;

	Job(JobFunc job, std::string name);

	// first iteration: allow having one parent to represent dependencies
	//Job(JobFunc job, std::string name, Job* parent);

	// allow to specify other jobs that define the dependants
	Job(JobFunc job, std::string name, std::vector<Job*> dependants);

	std::string GetName() const;

	std::int_fast32_t GetUnfinishedJobs() const
	{
		return mUnfinishedJobs.load();
	}

	// need something to check if dependencies are met
	// TODO: are there any other constraints so the Job can't be started?
	bool CanExecute() const;

	void Execute();

	bool IsFinished() const;

	void Finish();
};