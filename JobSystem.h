#pragma once
#include <atomic>
#include <vector>   
#include "JobQueue.h"



class JobSystem
{

public:
	//How many jobs are still open
	std::atomic<unsigned int> jobsToDo = 0;
	JobSystem(std::atomic<bool>& isRunning, int desiredThreadCount);
	//Stops the system and waits for all workers to join
	void JoinJobs();
	Job* CreateJob(JobFunction jobFunction);
	//Sets up the dependency connection between two jobs. Dependencies need to be set up before 
	//adding jobs to the system using AddJob
	void AddDependency(Job* dependent, Job* dependency);
	//Adds a job to the system. From this point it will be worked at some point (if dependencies are met).
	void AddJob(Job* job);
	//Wait until all jobs are finished
	void WaitForAllJobs();
	//Thread local stored id of the worker thread.
	__declspec(thread) static int thread_id;
private:

	std::atomic<bool>& isRunning;
	bool stopped = false;
	int current_queue_index = 0;
	std::condition_variable allJobsDoneConditionalVariable;
	std::vector<std::thread> workers;
	std::vector<JobQueue*> queues;

	void Worker(unsigned int id);
	bool TryToWorkJob();
	void WaitForAvailableJobs();
	JobQueue* GetQueue();
	Job* GetJob();
	void StealJob();
	bool CanExecuteJob(Job* job);
	void Execute(Job* job);
	void Finish(Job* job);
	void WakeAll();
};

