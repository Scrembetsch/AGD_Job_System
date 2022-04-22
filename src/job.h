#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <string>

class Job
{
private:
	// additional debug information by providing a readable task name
	std::string mName;

	// a list of dependants, allowing more constraints
	std::vector<Job*> mDependants;

	typedef void (*JobFunc)();
	JobFunc mJobFunction;

	// atomic type to ensure increment and decrement are visible to all threads
	// 0 means jobs done
	// 1 means "this" job has not finished yet
	// value > 1 means having open dependencies
	std::atomic_int_fast32_t mUnfinishedJobs{ 1 };

	// may use data and context of any kind but not supported in this exercise
	//void* mData;
	//void* mContext;

	// could add padding array to align to cache line size to prevent false sharing
	// char padding[CacheLineBytes(64) - JobFunc(8) - vector(24) - int32(4) - name(32)];
	// but because of debug features (name) we are already at 72 bytes and couldn't measure any
	// performance losses that would justify doing so
	// char padding[2*64 - 72];

public:
	// we don't support jobs with no worker function
	Job() = delete;

	// we also don't want Jobs to be copied because it doesn't make much sense
	// but also because handling internal atomics don't like this
	Job(const Job&) = delete;
	Job& operator =(const Job&) = delete;

	// also deleting move semantics otherwise would need to update dependent pointers
	Job(Job&&) = delete;
	Job& operator=(Job&&) = delete;

	Job(JobFunc job, std::string name);

	// allow to specify other jobs that define the dependants
	Job(JobFunc job, std::string name, std::vector<Job*> dependants);

	// need something to check if dependencies are met
	bool CanExecute() const;

	void Execute();

	bool IsFinished() const;

	void Finish();

	// debug functionality for printing additional information
	// should get stripped away by compiler if not used
	std::string GetName() const;

	std::int_fast32_t GetUnfinishedJobs() const;

	bool HasDependants() const;
};