#pragma once

class Job
{
public:
	typedef void (*JobFunc)();

	Job(JobFunc job)
		: JobFunction(job)
	{
	}

	JobFunc JobFunction;
};