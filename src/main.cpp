/*
 * Handl Anja <gs20m005>, Tributsch Harald <gs20m008>, Michael Leithner <gs20m012>

------------------------------------------------------------------------------------
open TODOs:
- notify when jobs are finished?
- bis 15.04. lockless (Michael)
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
#include "defines.h"
#include "job_system.h"

#include <thread>
#include <vector>
#include <atomic>
#include <iostream> // using cout to print thread_id, otherwise would need to create a stringstream


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
		HTL_LOG("job \"" << __func__ << "\" on thread #" << std::this_thread::get_id() << " done!"); \
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

	HTL_LOGD("All jobs done!");
}

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

#ifdef TEST_DEPENDENCIES
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
	// We also want to keep one available thread "free" so that OS has no problems to schedule main thread
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
	// We spawn a "main" thread so we can have the actual main thread blocking to receive a potential quit
	std::thread main_runner([ &isRunning, &jobSystem ]()
	{
		OPTICK_THREAD("Update");

		while ( isRunning )
		{
			OPTICK_FRAME("Frame");
			if ( isRunningParallel )
			{
				UpdateParallel(*jobSystem);
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

	HTL_LOG("starting execution in " << (isRunningParallel ? "parallel" : "serial") << " mode on main_runner thread #" << main_runner.get_id() << "...");
	HTL_LOG("Type anything to quit...");

	char c;
	scanf_s("%c", &c, 1);
	HTL_LOG("Quitting...");
	isRunning = false;

	HTL_LOG("shutting down all worker threads...");
	jobSystem->ShutDown();

	HTL_LOG("waiting for main_runner to join...");
	main_runner.join();

	OPTICK_SHUTDOWN();
}

