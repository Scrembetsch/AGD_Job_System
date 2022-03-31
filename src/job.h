#pragma once

#include "../optick/src/optick.h"

class Job
{
public:
	typedef void (*JobFunc)();

	Job()
		: JobFunction(nullptr)
	{
	}

	Job(JobFunc job)
		: JobFunction(job)
	{
	}

	JobFunc JobFunction;
};