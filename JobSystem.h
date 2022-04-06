#pragma once
#include <atomic>
#include <vector>
#include "JobQueue.h"



class JobSystem
{	

public:
	JobSystem(std::atomic<bool>& isRunning);
	void JoinJobs();
	void CreateJob(void(*jobFunction)());

private:
	std::atomic<bool>& isRunning;
	//TODO: probably should not use a vector here
	std::vector<std::thread> workers;
	JobQueue queue;

private:
	void Worker(unsigned int id);
	void WaitForAvailableJobs();
	Job* GetJob();
	bool CanExecuteJob(Job* job);
	void Execute(Job* job);
	void Finish(Job* job);
	void WorkOnOtherAvailableTask();
};

