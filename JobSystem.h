#pragma once
#include <atomic>
#include <vector>   
#include "JobQueue.h"



class JobSystem
{

public:
	std::atomic<unsigned int> jobsToDo = 0;
	JobSystem(std::atomic<bool>& isRunning, int desiredThreadCount);
	void JoinJobs();
	Job* CreateJob(JobFunction jobFunction);
	void AddDependency(Job* dependent, Job* dependency);
	void AddJob(Job* job);
	void WaitForAllJobs();
	__declspec(thread) static int thread_id;
private:

	std::condition_variable queuesNotEmptyConditionalVariable;
	std::condition_variable allJobsDoneConditionalVariable;
	std::atomic<bool>& isRunning;
	std::vector<std::thread> workers;
	std::vector<JobQueue*> queues;

	bool stopped = false;


	int current_queue_index = 0;
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

