#pragma once
#include <atomic>
#include <vector>   
#include "JobQueue.h"



class JobSystem
{

public:
	JobSystem(std::atomic<bool>& isRunning, int desiredThreadCount);
	void JoinJobs();
	Job* CreateJob(JobFunction jobFunction);
	void AddDependency(Job* dependent, Job* dependency);
	void AddJob(Job* job);
	__declspec(thread) static int thread_id;
private:
	std::atomic<bool>& isRunning;
	std::vector<std::thread> workers;
	JobQueue queue;

	bool stopped = false;

public:
	std::atomic<unsigned int> jobsToDo = 0;

private:
	void Worker(unsigned int id);
	void WaitForAvailableJobs();
	Job* GetJob();
	bool CanExecuteJob(Job* job);
	void Execute(Job* job);
	void Finish(Job* job);
	bool TryWorkingOnJob();
};

