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
	//Time was measured with the stress test flags (SIMULATENOUS_FRAME_COUNT & PARTICLE_JOB_COUNT) set to one as that would
	//be the default behaviour. This makes sense as with the way the dependencies are set up at most there will only be 3 jobs 
	//running at the same time. Even this case is quite unlikely, so during the majority of the time there will only be
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
	//spawn a worker and a queue for each thread
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
	//stop the jobsystem
	isRunning = false;
	//stop potentially picking up new jobs
	stopped = true;
	//wake up all threads
	WakeAll();
	//wait for each thread to join
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
	if (dependency->dependentCount > MAX_DEPENDENT_COUNT-1) {
		//We only support a max amount of dependcies so job struct stays the size of two cache lines to be cache friendly
		PRINT_ESSENTIAL(("Jobsystem only supports a max of " + std::to_string(MAX_DEPENDENT_COUNT) + " dependents.\n").c_str());
		exit(1);
	}
	else {
		// Add dependent to the job.
		dependency->dependents[dependency->dependentCount] = dependent;
		dependency->dependentCount++;
		// Increase dependency count, blocking this job until all dependencies are resolved
		dependent->dependencyCount++;
	}
}

void JobSystem::AddJob(Job* job)
{
	jobsToDo++;
	job->queue = queues[current_queue_index];
	queues[current_queue_index]->Push(job);
	//Current queue index gets increased with wrap around, thus jobs get added to queues in a round robing fashion.
	current_queue_index = ((current_queue_index + 1) % static_cast<int>(queues.size()));
}

//Waits until the jobsystem has no job left. This is used so a frame can wait for all it's jobs to be finished.
void JobSystem::WaitForAllJobs()
{
	std::unique_lock<std::mutex> lock(waitForAllJobMutex);
	//Predicate checks if jobsystem has no more jobs to do or it has stopped running.
	allJobsDoneConditionalVariable.wait(lock, [&]()
		{
			return !isRunning || jobsToDo==0;
		});
}

//The worker thread
void JobSystem::Worker(unsigned int id)
{
	OPTICK_THREAD(("WORKER #" + std::to_string(id)).c_str());
	//Set thread local variable to its specifc id, so it can be used to access thread specific queue.
	thread_id = id;
	while (isRunning)
	{
		WaitForAvailableJobs();
		if (!stopped) {
			//Try to work a job from its own queue.
			if (!TryToWorkJob()) {
				//If that did not work try to steal a job.
				StealJob();
				//And try to work that job.
				TryToWorkJob();
			}
		}
	}
	//Need to call this here otherwise we get stuck in ParallelUpdate if we quit early, as it waits for all jobs to end.
	allJobsDoneConditionalVariable.notify_all();
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
	//if we are stopped don't wait to allow exiting
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
	//Gets the thread specific queue using the thread local stored thread id
	return queues[thread_id];
}
Job* JobSystem::GetJob()
{
	PRINTW(thread_id, "GetJob");
	//Getting a job from the own queue uses the private end of it with Pop()
	Job* job = GetQueue()->Pop();
	return job;
}

void JobSystem::StealJob() {
	PRINTW(thread_id, "StealJob");
	//Use a random index to acces another queue
	int randomIndex = rand() % queues.size();
	//Are we actually accesing a foreign queue
	if(randomIndex!=thread_id){
		//Stealing uses the public end of the queue with Steal()
		auto job = GetQueue()->Steal();
		if (job) {
			//If we got a job we add it to the private end of our own queue
			job->queue = GetQueue();
			GetQueue()->Push(job);
		}
	}
}

bool JobSystem::CanExecuteJob(Job* job)
{
	PRINTW(thread_id, "CanExecuteJob");
	//Did we actually get a job
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
	//In a real world application, we would probably also allow for passing of data to the jobFunction.
	job->jobFunction();
}

void JobSystem::Finish(Job* job)
{
	PRINTW(thread_id, "Finish");
	//deleting the job also updates dependencies
	delete job;
	jobsToDo--;
	if (jobsToDo == 0) {
		//If we have no more jobs notify. (So frame can end.)
		allJobsDoneConditionalVariable.notify_all();
	}
}

void JobSystem::WakeAll() {
	for (int i = 0; i < queues.size(); ++i) {
		//Only need to use notify_one instead of all, as there is only one worker waiting per queue.
		queues[i]->NotifyOne();
	}
}
