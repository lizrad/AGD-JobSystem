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
	bool IsQueueEmpty();

private:
	std::atomic<bool>& isRunning;
	//TODO: probably should not use a vector here
	std::vector<std::thread> workers;
	JobQueue queue;

	//TODO: Should this be another mutex or the one from the queue?
	std::mutex jobSystemMutex;
	std::condition_variable wakeCondition;
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
	bool WorkOnOtherAvailableTask();
};

