/*
 * Handl Anja <gs20m005>, Tributsch Harald <gs20m008>, Michael Leithner <gs20m012>

------------------------------------------------------------------------------------
TODO:

0. get cpu and thread cores, cmd args (threads, isRunningParallel, ...)
bis 04.04.

1. easy scheduler for multiple worker with deques - Harald
- Jobs
- Job Manager (scheduler)
    - starts threads
    - notify of finished?
- worker
    - job deque with locks
    - get next job

2. dependencies - Michael
- dependency on other Jobs
    - can execute?
- can have children

3. job stealing - Anja
    - steal from other worker

bis 15.04.
4. lockless - Michael

------------------------------------------------------------------------------------

 */


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
#include "job_system.h"

#include <thread>
#include <atomic>
#include <iostream> // using cout to print thread_id, otherwise would need to create a stringstream

// Use this to switch between serial and parallel processing (for perf. comparison)
// configurable with cmd arg --parallel or -p
bool isRunningParallel = false;

// NO :(
using namespace std;

// custom defines for easier testing
// TODO: move together with macros from other files to defines.h file
//#define TEST_ONLY_ONE_FRAME // main loop returns after one execution
//#define TEST_DEPENDENCIES // test if correct dependencies are met

// Don't change this macros (unless for removing Optick if you want) - if you need something
// for your local testing, create a new one for yourselves.
#define MAKE_UPDATE_FUNC(NAME, DURATION) \
	void Update##NAME() { \
		OPTICK_EVENT(); \
		auto start = chrono::high_resolution_clock::now(); \
		decltype(start) end; \
		do { \
			end = chrono::high_resolution_clock::now(); \
		} while (chrono::duration_cast<chrono::microseconds>(end - start).count() < (DURATION)); \
		cout << "job \"" << __func__ << "\" on thread #" << this_thread::get_id() << " done!\n"; \
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

void UpdateSerial()
{
	OPTICK_EVENT();

	// Test if adding rendering first still respect dependencies
	UpdateRendering();
	UpdateInput();
	UpdatePhysics();
	UpdateCollision();
	UpdateAnimation();
	UpdateParticles();
	UpdateGameElements();
	UpdateSound();

#ifdef _DEBUG
	printf("All jobs done!\n");
#endif
}

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

	std::vector<Job*> jobs;

#ifdef TEST_DEPENDENCIES
	// Test if adding rendering first still respect dependencies
	Job rendering(&UpdateRendering, "rendering");
	Job collision(&UpdateCollision, "collision", rendering);
	Job physics(&UpdatePhysics, "physics", collision);
	// -> physics -> collision -> rendering
	jobSystem.AddJob(rendering);
#endif

	jobs.push_back(new Job(&UpdateInput, "input"));
#ifndef TEST_DEPENDENCIES
	jobs.push_back(new Job(&UpdatePhysics, "physics"));
	jobs.push_back(new Job(&UpdateCollision, "collision"));
#endif
	jobs.push_back(new Job(&UpdateAnimation, "animation"));
	jobs.push_back(new Job(&UpdateParticles, "particles"));
	jobs.push_back(new Job(&UpdateGameElements, "gameElements"));
	jobs.push_back(new Job(&UpdateSound, "sound"));
#ifdef TEST_DEPENDENCIES
	jobs.push_back(new AddJob(physics);
	jobs.push_back(new AddJob(collision);
#endif
	for (uint32_t i = 0; i < jobs.size(); i++)
	{
		jobSystem.AddJob(jobs[i]);
	}

	while (!jobSystem.AllJobsFinished());

#ifdef _DEBUG
	printf("All jobs done!\n");
#endif

	for (uint32_t i = 0; i < jobs.size(); i++)
	{
		delete jobs[i];
	}
	//jobs.clear();
	
}

uint32_t GetNumThreads(const ArgumentParser& argParser)
{
	const char* cShortArgName = "-t";
	const char* cLongArgName = "--threads";
	// hardwre_concurrency return value is only a hint, in case it returns < 2
	// we put a max() around it
	// We also want to keep one available thread "free" so that OS has no problems to schedule main thread
	uint32_t maxThreads = max(thread::hardware_concurrency(), 2U) - 1;

	uint32_t threads = maxThreads;
	if (argParser.CheckIfExists(cShortArgName, cLongArgName))
	{
		threads = argParser.GetInt(cShortArgName, cLongArgName);
		if (threads > maxThreads)
		{
			threads = maxThreads;
			printf("Specified number of threads is more than the available %d!\nDefaulting to: %d\n", maxThreads, threads);
		}
		else if (threads == 0)
		{
			threads = 1;
			printf("Need at least one worker!\nDefaulting to: %d\n", threads);
		}
		else
		{
			printf("Specified number of threads: %d\n", threads);

		}
	}
	else
	{
		printf("No number of cores specified.\nDefaulting to: %d\n", threads);
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
	cout << "starting execution in " << (isRunningParallel ? "parallel" : "serial") << " mode on main thread #" << this_thread::get_id() << "...\n";
	uint32_t numThreads = 1;
	if (isRunningParallel) {
		numThreads = GetNumThreads(argParser);
	}
	// TODO: no need to allocate JobSystem when running Serial
	JobSystem jobSystem(numThreads);

	atomic<bool> isRunning = true;
	// We spawn a "main" thread so we can have the actual main thread blocking to receive a potential quit
	thread main_runner([ &isRunning, &jobSystem ]()
	{
		OPTICK_THREAD("Update");

		while ( isRunning )
		{
			OPTICK_FRAME("Frame");
			if ( isRunningParallel )
			{
				UpdateParallel(jobSystem);
#ifdef TEST_ONLY_ONE_FRAME
				return;
#endif
			}
			else
			{
				UpdateSerial();
			}
		}
	});

	printf("Type anything to quit...\n");
	char c;
	scanf_s("%c", &c, 1);
	printf("Quitting...\n");
	isRunning = false;

	main_runner.join();

	OPTICK_SHUTDOWN();
}

