/*
 * Handl Anja <gs20m005>, Tributsch Harald <gs20m008>, Leithner Michael <gs20m012>
 * /
------------------------------------------------------------------------------------

// TODO:
// ===================
// - Measure and put results in README.md (nice to have)

// optional
// ------------
// (*) Fix LIFO / FIFO for private/public end (for better performance)
// (**) Allocate enough memory from outside and provide, allow resize/shrink

// further improvements
// ------------------------
// - Notify when jobs are finished
// - Enable lockless variant via parameter instead of macro
// - Do we want to use threadlocal variables or are we fine with our implementation?


// Problems and take aways:
// ==========================
// o Dependencies on jobs caused us a lot of headache xD
//    "Normal" push an pop could be easily implemented, but because of dependencies it could happen
//    that one queue encapsulates a depending, executable job between two dependant ones:
//          front -> [job #1 depends on job #0] [job #0] [job #2 depends on job #1] <- back
//    With the usual pop front on private end and pop back from public end available for other workers
//    this situation leads to a deadlock!
// -------------------------------------------------------------------------------------------------------
//    In a first iteration, jobs where sorted by open dependencies and pushed to worker queues in a order
//    so that they can be executed, but because of possible cpu reorderings this solution was not stable.
// -------------------------------------------------------------------------------------------------------
//    A second attempt was, if a worker pops a job and determines it is not executable,
//    the job is than pushed back at the back of the queue.
//    While this looked like a promising implementation, it could happen that one worker keeps reordering
//    its open jobs, thus exceeding the allocated memory boundaries
// -------------------------------------------------------------------------------------------------------
//    To eliminate the growing boundary counters it was necessary to reset them after each successful main frame
//    but this was not enough because if other threads work longer on jobs which have dependencies,
//    the reordering can still happen on the current active thread
//    A (hopefully) final solution was to reset the boundaries every time the queue pops its last item.

// o Another Problem were spurious wakeups, where the workers gets notified by the conditional and want to start
//    working because their queue size > 0. But as they try to get an executable job, it turns out all of them are not executable
//    because of open dependencies and can be observed building without our WAIT_FOR_AVAILABLE_JOBS macro in the logs:
//       Yield on thread #4
//       Yield on thread #4
//       Yield on thread #4
//       Yield on thread #4
//       ...
//    This means, a thread gets woken up because its conditional var is fulfilled, then tries to get a job.
//    No job is currently executable because of dependencies, so the worker yields and gives back his slot.
//    But then the system wakes the same thread up and again checks the conditional etc...
//
//    By using a build with WAIT_FOR_AVAILABLE_JOBS enabled, it can be observed that no thread is woken up without work to do :D
//    Looking at the performance measured with the optick profiler in release build with 7 threads, the results were almost identical,
//    with each frame almost always achieving the theoretically fastest execution time taking 5.7 ms
//
// ------------------------------------------------------------------------------------


 /* ===============================================
 * Optick is a free, light-weight C++ profiler
 * We use it in this exercise to make it easier for
 * you to profile your jobsystem and see what's
 * happening (dependencies, locking, comparison to
 * serial, ...)
 *
 * It's not mandatory to use it, just advised. If
 * you dislike it or have problems with it, feel
 * free to remove it from the solution
 * ===============================================*/
#include "../optick/src/optick.h"

#include "argument_parser.h"
#include "defines.h"
#include "job_system.h"


// Use this to switch between serial and parallel processing (for perf. comparison)
// configurable with cmd arg --parallel or -p
bool isRunningParallel = false;

// Don't change this macros (unless for removing Optick if you want) - if you need something
// for your local testing, create a new one for yourselves.
#define MAKE_UPDATE_FUNC(NAME, DURATION) \
	void Update##NAME() { \
		OPTICK_EVENT(); \
		auto start = std::chrono::high_resolution_clock::now(); \
		decltype(start) end; \
		do { \
			end = std::chrono::high_resolution_clock::now(); \
		} while (std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() < (DURATION)); \
		HTL_LOGD("Job \"" << __func__ << "\" on thread #" << std::this_thread::get_id() << " done!"); \
	} \

// You can create other functions for testing purposes but those here need to run in your job system
// The dependencies are noted on the right side of the functions, the implementation should be able to set them up so they are not violated and run in that order!
MAKE_UPDATE_FUNC(Input, 200) // no dependencies
MAKE_UPDATE_FUNC(Physics, 1000) // depends on Input
MAKE_UPDATE_FUNC(Collision, 1200) // depends on Physics
MAKE_UPDATE_FUNC(Animation, 600) // depends on Collision
MAKE_UPDATE_FUNC(Particles, 800) // depends on Collision
MAKE_UPDATE_FUNC(GameElements, 2400) // depends on Physics
MAKE_UPDATE_FUNC(Rendering, 2000) // depends on Animation, Particles, GameElements
MAKE_UPDATE_FUNC(Sound, 1000) // no dependencies
// ----------------------------------------------------------------------------------------------
// sum of all threads in serial would be 9200 microsec:
//	input	physics		collision	gameElements	animation	particles	sound		Rendering
//	200		-> 1000		-> 1200		-> 2400			-> 600		-> 800		-> 1000		-> 2000
// ----------------------------------------------------------------------------------------------
// due to dependencies, can be completed after 5800 microsec at earliest in parallel on 2 threads:
//	input	physics		gameElements						Rendering
//	200		-> 1000		-> 2400								-> 2000
//	sound				collision	animation	particles
//	1000				-> 1200		-> 600		-> 800
// ----------------------------------------------------------------------------------------------
// using 3 threads the frame can be completed fastest after 5600 microsec:
//	input	physics		gameElements						Rendering
//	200		-> 1000		-> 2400								-> 2000
//	sound				collision	animation
//	1000				-> 1200		-> 600
//									particles
//									-> 800
// ----------------------------------------------------------------------------------------------

void UpdateSerial()
{
	OPTICK_EVENT();

	// Test if adding rendering first still respect order
	UpdateRendering();
	UpdateInput();
	UpdatePhysics();
	UpdateCollision();
	UpdateAnimation();
	UpdateParticles();
	UpdateGameElements();
	UpdateSound();

	HTL_LOGD("All jobs done!");
}

// defined here, to avoid separate .cpp file for logger
std::mutex ThreadSafeLogger::mMutex;
ThreadSafeLogger ThreadSafeLogger::Logger;

/*
* ===============================================================
* In `UpdateParallel` you should use your jobsystem to distribute
* the tasks to the job system. You can add additional parameters
* as you see fit for your implementation (to avoid global state)
* ===============================================================
*/
void UpdateParallel(JobSystem& jobSystem)
{
	OPTICK_EVENT();

	HTL_LOGD("---------- CREATING JOBS ----------");
	std::vector<Job*> jobs;

#ifdef HTL_TEST_DEPENDENCIES
	// Test if adding rendering first still respect dependencies
	Job* sound = new Job(&UpdateSound, "sound");
	Job* rendering = new Job(&UpdateRendering, "rendering");
	Job* animation = new Job(&UpdateAnimation, "animation", { rendering });
	Job* gameElements = new Job(&UpdateGameElements, "gameElements", { rendering });
	Job* particles = new Job(&UpdateParticles, "particles", { rendering });
	Job* collision = new Job(&UpdateCollision, "collision", { animation, particles });
	Job* physics = new Job(&UpdatePhysics, "physics", { collision, gameElements });
	Job* input = new Job(&UpdateInput, "input", { physics });

	jobs.push_back(rendering);
	jobs.push_back(collision);
	jobs.push_back(physics);
	jobs.push_back(input);
	jobs.push_back(animation);
	jobs.push_back(particles);
	jobs.push_back(gameElements);
	jobs.push_back(sound);
#else
	jobs.push_back(new Job(&UpdateRendering, "rendering"));
	jobs.push_back(new Job(&UpdateCollision, "collision"));
	jobs.push_back(new Job(&UpdatePhysics, "physics"));
	jobs.push_back(new Job(&UpdateInput, "input"));
	jobs.push_back(new Job(&UpdateAnimation, "animation"));
	jobs.push_back(new Job(&UpdateParticles, "particles"));
	jobs.push_back(new Job(&UpdateGameElements, "gameElements"));
	jobs.push_back(new Job(&UpdateSound, "sound"));
#endif

#ifdef HTL_SORT_JOBS
	// sorting jobs by dependency so every thread can start directly
	// and there are no spurious wakeups, where a thread awakes because the queue is not empty
	// but than can complete no work because all jobs have open dependencies
	std::sort(jobs.begin(), jobs.end(), [](Job* l, Job* r) {
		// (*) different implementation until LIFO / FIFO order is unified
		#ifdef HTL_USING_LOCKLESS
			return l->GetUnfinishedJobs() < r->GetUnfinishedJobs();
		#else
			return l->GetUnfinishedJobs() > r->GetUnfinishedJobs();
		#endif
	});
#endif

	for (uint32_t i = 0; i < jobs.size(); i++)
	{
		jobSystem.AddJob(jobs[i]);
	}

	while (!jobSystem.AllJobsFinished());
	HTL_LOGD("All jobs done on main thread #" << std::this_thread::get_id() << "...");

	HTL_LOGD("---------- DELETING JOBS ----------");
	for (uint32_t i = 0; i < jobs.size(); i++)
	{
		delete jobs[i];
	}
}

uint32_t GetNumThreads(const ArgumentParser& argParser)
{
	const char* cShortArgName = "-t";
	const char* cLongArgName = "--threads";
	// hardware_concurrency return value is only a hint, in case it returns < 2 so we put a max() around it
	// we also want to keep one available thread "free" so that OS has no problems to schedule main thread
	// if we use multiple threads per logical cpu core, we would trash our cache
	uint32_t maxThreads = std::max(std::thread::hardware_concurrency(), 2U) - 1;

	uint32_t threads = maxThreads;
	if (argParser.CheckIfExists(cShortArgName, cLongArgName))
	{
		threads = argParser.GetInt(cShortArgName, cLongArgName);
		if (threads > maxThreads)
		{
			threads = maxThreads;
			HTL_LOG("Specified number of threads is more than the available " << maxThreads << "! Defaulting to : " << threads << "\n");
		}
		else if (threads == 0)
		{
			threads = 1;
			HTL_LOG("Need at least one worker! Defaulting to: " << threads);
		}
		else
		{
			HTL_LOG("Specified number of threads: " << threads);
		}
	}
	else
	{
		HTL_LOG("No number of cores specified. Defaulting to: " << threads);
	}
	return threads;
}

int main(int argc, char** argv)
{
	/*
	* =======================================
	* Setting memory allocator for Optick
	* In a real setting this could use a
	* specific debug-only allocator with
	* limits
	* =======================================
	*/
	OPTICK_SET_MEMORY_ALLOCATOR(
		[](size_t size) -> void* { return operator new(size); },
		[](void* p) { operator delete(p); },
		[]() { /* Do some TLS initialization here if needed */ }
	);
	OPTICK_THREAD("MainThread");

	ArgumentParser argParser(argc, argv);
	isRunningParallel = argParser.CheckIfExists("-p", "--parallel") ? true : isRunningParallel;
	JobSystem* jobSystem = nullptr; // no need to allocate JobSystem when running Serial
	uint32_t numThreads = 1;
	if (isRunningParallel) {
		numThreads = GetNumThreads(argParser);
		jobSystem = new JobSystem(numThreads);
	}

	std::atomic<bool> isRunning = true;
	// we spawn a "main" thread so we can have the actual main thread blocking to receive a potential quit
	std::thread main_runner([ &isRunning, &jobSystem ]()
	{
		OPTICK_THREAD("Update");

		while ( isRunning )
		{
			OPTICK_FRAME("Frame");
			if ( isRunningParallel )
			{
				UpdateParallel(*jobSystem);
#ifdef HTL_TEST_ONLY_ONE_FRAME
				return;
#endif
			}
			else
			{
				UpdateSerial();
			}
		}
	});

	HTL_LOG("Starting execution in " << (isRunningParallel ? "parallel" : "serial") << " mode on main_runner thread #" << main_runner.get_id() << "...");
	HTL_LOG("Type anything to quit...");

	char c;
	scanf_s("%c", &c, 1);
	if (c == 'd')
	{
		HTL_LOG("Debug info:");
		for (size_t i = 0; i < jobSystem->GetNumWorkers(); ++i)
		{
			jobSystem->GetWorkers()[i].Print();
		}
	}
	HTL_LOG("Quitting...");
	isRunning = false;

	HTL_LOG("Shutting down all worker threads...");
	jobSystem->ShutDown();

	HTL_LOG("Waiting for main_runner to join...");
	main_runner.join();

	OPTICK_SHUTDOWN();
}

