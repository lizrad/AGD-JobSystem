#pragma once
#include <queue>
#include <mutex>

typedef void (*JobFunction)();

struct Job
{
	JobFunction jobFunction; // 4 Byte?
	// Number of current dependencies to other jobs (which this job has to wait for)
	std::atomic<unsigned int> dependencyCount = 0;
	// Number of dependents of this job
	unsigned int dependentCount = 0;
	// Jobs that depend on this job
	Job* dependents[16];
	//TODO: still need: data(?), padding to be an exact multiple of cache line size
};

//JobQueue manages thread save access to a queue using a mutex.
class JobQueue
{
private:
	std::queue<Job*> queue;
	std::mutex queueMutex;

public:
	void Push(Job* job);
	Job* Pop();
	bool IsEmpty();
};

