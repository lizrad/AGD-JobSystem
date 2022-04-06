/*
 * Amon Ahmed (gs20m014), Karl Bittner (gs20m013), David Panagiotopulos (gs20m019)
 */

 /*
#Exercise 2 - Implement a multi-threaded jobsystem

##Tasks
-[x] Implement a schedule-based jobsystem in C++
-[x] Support job dependency configuration(Physics -> Collision -> Rendering, as stated in the assignment template).
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

#include "Settings.h"
#include "JobSystem.h"

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
	Job* updateInputJob = jobsystem.CreateJob(&UpdateInput);
	Job* updatePhysicsJob = jobsystem.CreateJob(&UpdatePhysics);
	Job* updateCollisionJob = jobsystem.CreateJob(&UpdateCollision);
	Job* updateAnimationJob = jobsystem.CreateJob(&UpdateAnimation);
	Job* updateParticlesJob = jobsystem.CreateJob(&UpdateParticles);
	Job* updateGameElementsJob = jobsystem.CreateJob(&UpdateGameElements);
	Job* updateRenderingJob = jobsystem.CreateJob(&UpdateRendering);
	Job* updateSoundJob = jobsystem.CreateJob(&UpdateSound);

	jobsystem.AddDependency(updatePhysicsJob, updateInputJob);
	jobsystem.AddDependency(updateCollisionJob, updatePhysicsJob);
	jobsystem.AddDependency(updateAnimationJob, updateCollisionJob);
	jobsystem.AddDependency(updateParticlesJob, updateCollisionJob);
	jobsystem.AddDependency(updateGameElementsJob, updatePhysicsJob);

	jobsystem.AddDependency(updateRenderingJob, updateAnimationJob);
	jobsystem.AddDependency(updateRenderingJob, updateParticlesJob);
	jobsystem.AddDependency(updateRenderingJob, updateGameElementsJob);

	jobsystem.AddJob(updateRenderingJob);
	jobsystem.AddJob(updatePhysicsJob);
	jobsystem.AddJob(updateAnimationJob);
	jobsystem.AddJob(updateInputJob);
	jobsystem.AddJob(updateCollisionJob);
	jobsystem.AddJob(updateGameElementsJob);
	jobsystem.AddJob(updateParticlesJob);
	jobsystem.AddJob(updateSoundJob);
}

int main()
{
#ifdef CAPTURE_OPTICK
	OPTICK_START_CAPTURE();
#endif // CAPTURE_OPTICK
	/*
	* =======================================
	* Setting memory allocator for Optick
	* In a real setting this could use a
	* specific debug-only allocator with
	* limits
	* =======================================
	*/
	OPTICK_SET_MEMORY_ALLOCATOR(
		[](size_t size) -> void*
		{
			return operator new(size);
		},
		[](void* p)
		{
			operator delete(p);
		},
			[]()
		{ /* Do some TLS initialization here if needed */
		}
		);

	atomic<bool> isRunning = true;
	OPTICK_THREAD("MainThread");
	// We spawn a "main" thread so we can have the actual main thread blocking to receive a potential quit
	thread main_runner([&isRunning]()
		{
			JobSystem jobsystem(isRunning);
			OPTICK_THREAD("Update");
			while (isRunning)
			{
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
				//TODO: As isRunning cancels everything, this just waits a bit for all jobs to finish
				auto start = chrono::high_resolution_clock::now(); \
				decltype(start) end;
				do
				{
					end = chrono::high_resolution_clock::now();
				}
				while (chrono::duration_cast<chrono::microseconds>(end - start).count() < (300000));
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

#ifdef CAPTURE_OPTICK
	OPTICK_STOP_CAPTURE();
	OPTICK_SAVE_CAPTURE("capture.opt")
#endif // CAPTURE_OPTICK

		OPTICK_SHUTDOWN();
}

