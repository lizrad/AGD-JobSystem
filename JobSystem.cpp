#include "JobSystem.h"
#include <stdlib.h>
#include <algorithm>
#include <mutex>
#include <string>
#include "optick_src/optick.h"
#include "Settings.h"

int JobSystem::thread_id = -1;

JobSystem::JobSystem(std::atomic<bool>& isRunning, int desiredThreadCount) : isRunning(isRunning)
{
	//using hardware core count - 1 because we already use one thread for the main runner
	//using max function because hardware_concurrency might return 0 if it cannot read hardware specs 
	unsigned int available_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
	PRINT(("Max Available Threads: " + std::to_string(available_threads) + "\n").c_str());
	//Through measuring we found out that for this very specific exercise the optimal amount of threads 
	//would be 3 as any further thread does not really decrease the time spent. See measuring output below:
	/*
	Average parallel frame time for input thread count of: 1:       9310147.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 2:       5893307.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 3:       5680801.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 4:       5695426.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 5:       5697207.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 6:       5700832.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 7:       5685485.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 8:       5682539.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 9:       5678031.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 10:      5681702.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 11:      5686856.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 12:      5689230.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 13:      5677641.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 14:      5691101.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 15:      5692206.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 16:      5701897.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 17:      5710286.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 18:      5686505.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 19:      5696779.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 20:      5696573.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 21:      5701224.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 22:      5702335.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 23:      5691203.000000ns. (Averaged over 100 frames.)
	Average parallel frame time for input thread count of: 24:      5688854.000000ns. (Averaged over 100 frames.)
	*/
	//This makes sense as with the way the dependencies are set up at most there will only be 3 jobs running at
	//the same time. Even this case is quite unlikely, so during the majority of the time there will only be
	//1-2 jobs running concurrently, explaining the relatively little time gain when going from 2 to 3 threads.
	//This optimum is VERY specific to this demo program, so in a real life setting the default optimum would most likely
	//be equal to the maximum available hardware threads (or even exceed it, when for example doing a lot of IO operations). 
	//At the very least another speed test after settling on an implementation would be necessary to find a new sensible
	//optimum as a real world application would not allow for such easy  guesstimation concerning the maxmimum concurrent jobs.
	unsigned int optimal_threads = std::min(available_threads, 3u);
	unsigned int thread_count = optimal_threads;
	//If a user input for the thread count was specified
	if (desiredThreadCount > 0) {
		//We are capping against the maximum available threads here. BUT we are not sure if this is what was desired 
		//in this exercise. It would also make sense to cap against the optimum but as this is very specfic for this 
		//demo program it might not be what the user wants. In a real world setting we thought it would make more sense
		//to give the user as much freedom as possible. 
		if (static_cast<unsigned int>(desiredThreadCount) <= available_threads) {
			thread_count = desiredThreadCount;
		}
	}
	for (unsigned int core = 0; core < thread_count; ++core)
	{
		PRINT(("CREATING WORKER FOR CORE " + std::to_string(core) + "\n").c_str());
		JobQueue* queue = new JobQueue(isRunning);
		queues.push_back(queue);
		workers.push_back(std::thread(&JobSystem::Worker, this, core));
	}
}

void JobSystem::JoinJobs()
{
	isRunning = false;
	
		stopped = true;
		WakeAll();
	for (auto& worker : workers)
	{
		worker.join();
	}
}

Job* JobSystem::CreateJob(JobFunction jobFunction)
{
	Job* job = new Job;
	job->jobFunction = jobFunction;
	return job;
}

void JobSystem::AddDependency(Job* dependent, Job* dependency)
{
	if (dependency->dependentCount > MAX_DEPENDENT_COUNT) {
		PRINT_ESSENTIAL(("Jobsystem only supports a max of " + std::to_string(MAX_DEPENDENT_COUNT) + " dependents.\n").c_str());
		exit(1);
	}
	// Add dependent to the job
	dependency->dependents[dependency->dependentCount] = dependent;
	dependency->dependentCount++;
	// Increase dependency count, blocking this job until all dependencies are resolved
	dependent->dependencyCount++;
}

void JobSystem::AddJob(Job* job)
{
	jobsToDo++;
	job->queue = queues[current_queue_index];
	queues[current_queue_index]->Push(job);
	current_queue_index = ((current_queue_index + 1) % static_cast<int>(queues.size()));
}

void JobSystem::WaitForAllJobs()
{
	std::mutex mutex;
	std::unique_lock<std::mutex> lock(mutex);
	allJobsDoneConditionalVariable.wait(lock, [&]()
		{
			return !isRunning || jobsToDo==0;
		});
}

void JobSystem::Worker(unsigned int id)
{
	thread_id = id;
	OPTICK_THREAD(("WORKER #" + std::to_string(id)).c_str());
	while (isRunning)
	{
		WaitForAvailableJobs();
		//Try to work a job
		if (!TryToWorkJob()) {
			StealJob();
			TryToWorkJob();
		}
	}
	PRINTW(thread_id, "Exiting...");
}
bool JobSystem::TryToWorkJob() {
	auto job = GetJob();
	if (CanExecuteJob(job))
	{
		Execute(job);
		Finish(job);
		return true;
	}
	return false;
}
void JobSystem::WaitForAvailableJobs()
{
	PRINTW(thread_id, "WaitForAvailableJobs");
	if (!stopped)
	{
		// If there is nothing else to do, go to sleep
		PRINTW(thread_id, "Sleeping...");
		GetQueue()->WaitForJob();
		PRINTW(thread_id, "Waking...");
	}
}

JobQueue* JobSystem::GetQueue() {
	PRINTW(thread_id, "GetQueue");
	return queues[thread_id];
}
Job* JobSystem::GetJob()
{
	PRINTW(thread_id, "GetJob");
	Job* job = GetQueue()->Pop();
	return job;
}

void JobSystem::StealJob() {
	PRINTW(thread_id, "StealJob");
	int randomIndex = rand() % queues.size();
	auto job = GetQueue()->Steal();
	if (job) {
		job->queue = GetQueue();
	}
	GetQueue()->Push(job);
}

bool JobSystem::CanExecuteJob(Job* job)
{
	PRINTW(thread_id, "CanExecuteJob");
	if (job)
	{
		//This jobs still depends on another job finishing
		if (job->dependencyCount > 0)
		{
			// Add job back to queue for later
			GetQueue()->Push(job);
			return false;
		}
		else
		{
			return true;
		}
	}
	return false;
}

void JobSystem::Execute(Job* job)
{
	PRINTW(thread_id, "Execute");
	job->jobFunction();
}

void JobSystem::Finish(Job* job)
{
	PRINTW(thread_id, "Finish");
	delete job;
	jobsToDo--;
	if (jobsToDo == 0) {
		allJobsDoneConditionalVariable.notify_all();
	}
}

void JobSystem::WakeAll() {
	for (int i = 0; i < queues.size(); ++i) {
		queues[i]->NotifyOne();
	}

}
