/*
 * Amon Ahmed (gs20m014), Karl Bittner (gs20m013), David Panagiotopulos (gs20m019)
 */

 /*
#Exercise 2 - Implement a multi-threaded jobsystem

##Tasks
-[] Implement a schedule-based jobsystem in C++
-[] Support job dependency configuration(Physics -> Collision -> Rendering, as stated in the assignment template).
	It should be allowed to re-configure dependencies when adding a job to the scheduler(no runtime switching necessary)
-[] Allow the jobsystem to be configured in regards to how many threads it can use and automatically detect how many
	threads are available on the target CPU.
-[] Choose an ideal worker thread count based on the available CPU threads and comment why you choose that number.
-[] Ensure the correctness of the program(synchronization, C++ principles, errors, warnings)
-[] Write a short executive summary on how your jobsystem is supposed to work and which features it supports
	(work-stealing, lock-free, ...) This can be either max. 1 A4 page additionally, a readme or comments in the source
	code (but if they are comments, please be sure they explain your reasoning for doing something)

##Bonus points
-[] Implement the work-stealing algorithm to use the available resources more efficiently(+10 pts)
-[] Implement the work-stealing queue(requires the above bonus task) with lock - free mechanisms (+5 pts, but please
	really only try this if you feel confident and if your solution is already working with locks, don not go for this for
	the first iteration)
-[] Allow configuration of the max worker thread count via command-line parameters and validate them against available
	threads(capped) (+2 pts)
 */

#include <cstdio>
#include <cstdint>
#include <thread>
#include <queue>
#include <algorithm>
#include <string>
#include <mutex>
#include <atomic>
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
#include "optick_src/optick.h"

using namespace std;

// Use this to switch betweeen serial and parallel processing (for perf. comparison)
constexpr bool isRunningParallel = true;

#define RUN_ONCE

#define VERBOSE

#ifdef VERBOSE
#define PRINT(x) printf(x)
#else
#define PRINT(x)
#endif

// Don't change this macros (unlsess for removing Optick if you want) - if you need something
// for your local testing, create a new one for yourselves.
#define MAKE_UPDATE_FUNC(NAME, DURATION) \
	void Update##NAME() { \
		OPTICK_EVENT(); \
		PRINT(#NAME "\n"); \
		auto start = chrono::high_resolution_clock::now(); \
		decltype(start) end; \
		do { \
			end = chrono::high_resolution_clock::now(); \
		} while (chrono::duration_cast<chrono::microseconds>(end - start).count() < (DURATION)); \
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
	PRINT("Serial\n");
	UpdateInput();
	UpdatePhysics();
	UpdateCollision();
	UpdateAnimation();
	UpdateParticles();
	UpdateGameElements();
	UpdateRendering();
	UpdateSound();
}

struct Job {
	Job(void(*jobFunction)()) :jobFunction(jobFunction) {}
	void(*jobFunction)();
	//TODO: still need: data(?), padding to be an exact multiple of cache line size
};

class JobSystem {

public:
	JobSystem(atomic<bool>& isRunning) : isRunning(isRunning) {
		//using max bc hardware_concurrency might return 0 if it cannot read hardware specs 
		//TODO: should we actually use one core less here bc main already uses one?
		//TODO: also make this configurable if the user desires
		const unsigned int core_count = max(1u, std::thread::hardware_concurrency());
		//TODO: how many worker are optimal for the core count (for now it just uses 1 to 1)
		for (unsigned int core = 0; core < core_count; ++core) {
			PRINT(("CREATING WORKER FOR CORE " + to_string(core) + "\n").c_str());
			workers.push_back(thread(&JobSystem::Worker, this, core));
		}
	}
	void JoinJobs() {
		for (auto& worker : workers) {
			worker.join();
		}
	}
	void CreateJob(void(*jobFunction)()) {
		//TODO: add way to specify dependencies
		Job* job = new Job(jobFunction);
		queue.Push(job);
	}

private:
	//JobQueue manages thread save access to a queue using a mutex.
	class JobQueue {
		queue<Job*> queue;
		mutex mutex;
		//TODO: probably also need a conditional variable here to notify when work is available
	public:
		void Push(Job* job) {
			lock_guard<std::mutex> guard(mutex);
			queue.push(job);
		}

		Job* Pop() {
			lock_guard<std::mutex> guard(mutex);
			if (queue.empty()) {
				return nullptr;
			}
			Job* job = queue.front();
			queue.pop();
			return job;
		}

		bool IsEmpty() {
			lock_guard<std::mutex> guard(mutex);
			return queue.empty();
		}
	};




	atomic<bool>& isRunning;
	//TODO: probably should not use a vector here
	vector<thread> workers;
	JobQueue queue;

	void Worker(unsigned int id) {
		while (isRunning) {

			PRINT(("WORKER #" + to_string(id) + ": WaitForAvailableJobs\n").c_str());
			WaitForAvailableJobs();
			PRINT(("WORKER #" + to_string(id) + ": GetJob\n").c_str());
			Job* job = GetJob();
			PRINT(("WORKER #" + to_string(id) + ": CanExecuteJob\n").c_str());
			if (CanExecuteJob(job))
			{
				PRINT(("WORKER #" + to_string(id) + ": Execute\n").c_str());
				Execute(job);
				PRINT(("WORKER #" + to_string(id) + ": Finish\n").c_str());
				Finish(job);
			}
			else
			{
				PRINT(("WORKER #" + to_string(id) + ": WorkOnOtherAvailableTask\n").c_str());
				WorkOnOtherAvailableTask();
			}
		}
	}
	void WaitForAvailableJobs() {
		//TODO: Wait until Queue has Job 
	}
	Job* GetJob() {
		return queue.Pop();
	}

	bool CanExecuteJob(Job* job) {
		if (job) {
			//TODO: Check for dependencies
			return true;
		}
		return false;
	}

	void Execute(Job* job) {
		job->jobFunction();
	}

	void Finish(Job* job) {
		//TODO: Mark job as resolved for dependencies and wait for child jobs(?)
	}

	void WorkOnOtherAvailableTask() {
		//TODO: not sure, must get new job from queue and put old one back, but how often do we try etc.?
	}
};





/*
* ===============================================================
* In `UpdateParallel` you should use your jobsystem to distribute
* the tasks to the job system. You can add additional parameters
* as you see fit for your implementation (to avoid global state)
* ===============================================================
*/
void UpdateParallel(JobSystem& jobsystem)
{
	OPTICK_EVENT();
	PRINT("Parallel\n");
	jobsystem.CreateJob(&UpdateInput);
	jobsystem.CreateJob(&UpdatePhysics);
	jobsystem.CreateJob(&UpdateCollision);
	jobsystem.CreateJob(&UpdateAnimation);
	jobsystem.CreateJob(&UpdateParticles);
	jobsystem.CreateJob(&UpdateGameElements);
	jobsystem.CreateJob(&UpdateRendering);
	jobsystem.CreateJob(&UpdateSound);
}

int main()
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

	atomic<bool> isRunning = true;
	OPTICK_THREAD("MainThread");
	// We spawn a "main" thread so we can have the actual main thread blocking to receive a potential quit
	thread main_runner([&isRunning]()
		{
			JobSystem jobsystem(isRunning);
			OPTICK_THREAD("Update");
			while (isRunning) {
				OPTICK_FRAME("Frame");
				if (isRunningParallel)
				{
					UpdateParallel(jobsystem);
				}
				else
				{
					UpdateSerial();
				}
#ifdef RUN_ONCE
				isRunning = false;
#endif // RUN_ONCE
			}
			jobsystem.JoinJobs();
		});
#ifndef  RUN_ONCE
	printf("Type anything to quit...\n");
	char c;
	scanf_s("%c", &c, 1);
	printf("Quitting...\n");
	isRunning = false;
#endif // ! RUN_ONCE

	main_runner.join();

	OPTICK_SHUTDOWN();
	}

