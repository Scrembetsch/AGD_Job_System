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

#include <cstdio>
#include <cstdint>
#include <thread>
#include <atomic>
#include <stdio.h>

// Use this to switch between serial and parallel processing (for perf. comparison)
constexpr bool isRunningParallel = true;

/*
* ===============================================
* Optick is a free, light-weight C++ profiler
* We use it in this exercise to make it easier for
* you to profile your jobsystem and see what's
* happening (dependencies, locking, comparison to
* serial, ...)
*
* It's not mandatory to use it, just advised. If
* you dislike it or have problems with it, feel
* free to remove it from the solution
* ===============================================
*/
#include "../optick/src/optick.h"
#include "argument_parser.h"
#include "job_system.h"

// NO :(
using namespace std;

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
		printf("Jobs done! %s\n", __func__); \
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

void UpdateCustom()
{
	OPTICK_EVENT();
	auto start = chrono::high_resolution_clock::now(); \
	decltype(start) end; \
	do {
		\
			end = chrono::high_resolution_clock::now(); \
	} while (chrono::duration_cast<chrono::microseconds>(end - start).count() < (500)); \
	printf("Jobs done! %s\n", __func__); \
}

void UpdateSerial()
{
	OPTICK_EVENT();

	UpdateInput();
	UpdatePhysics();
	UpdateCollision();
	UpdateAnimation();
	UpdateParticles();
	UpdateGameElements();
	UpdateRendering();
	UpdateSound();
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

	jobSystem.AddJob(Job(&UpdateCustom));
	jobSystem.AddJob(Job(&UpdateInput));
	jobSystem.AddJob(Job(&UpdatePhysics));
	jobSystem.AddJob(Job(&UpdateCollision));
	jobSystem.AddJob(Job(&UpdateAnimation));
	jobSystem.AddJob(Job(&UpdateParticles));
	jobSystem.AddJob(Job(&UpdateGameElements));
	jobSystem.AddJob(Job(&UpdateRendering));
	jobSystem.AddJob(Job(&UpdateSound));

	while (!jobSystem.AllJobsFinished())
	{
		bool t = false;
	}
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
	uint32_t numThreads = GetNumThreads(argParser);
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

