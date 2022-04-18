#pragma once
#include <deque>
#include <mutex>

typedef void (*JobFunction)();

struct Job
{	
	JobFunction jobFunction = nullptr; // 4 Byte?
	// Number of current dependencies to other jobs (which this job has to wait for)
	std::atomic<unsigned int> dependencyCount = 0;
	// Number of dependents of this job
	unsigned int dependentCount = 0;
	// Jobs that depend on this job
	Job* dependents[16] = {};
	//TODO: still need: data(?), padding to be an exact multiple of cache line size
};

//JobQueue manages thread save access to a queue using a mutex.
class JobQueue
{
public:
	JobQueue(std::atomic<bool>& isRunning);
	void Push(Job* job);
	Job* Pop();
	Job* Steal();
	void WaitForJob();
	void NotifyAll();
	void NotifyOne();
private:
	std::deque<Job*> deque;
	std::mutex queueMutex;
	std::atomic<bool>& isRunning;
	std::condition_variable queueConditionalVariable;
};

