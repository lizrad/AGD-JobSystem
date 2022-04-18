#pragma once
#include <deque>
#include <mutex>
#include "Settings.h"

#define MAX_DEPENDENT_COUNT 13
typedef void (*JobFunction)();

struct Job;

//JobQueue manages thread save access to a queue using a mutex.
class JobQueue
{
public:
	JobQueue(std::atomic<bool>& isRunning);
	void Push(Job* job);
	Job* Pop();
	Job* Steal();
	bool IsEmpty();
	void WaitForJob();
	void NotifyOne();
private:
	std::deque<Job*> deque;
	std::mutex mutex;
	std::atomic<bool>& isRunning;
	std::condition_variable conditionalVariable;
};

struct Job
{
	JobFunction jobFunction = nullptr; // 8 Bytes (assumed not guaranteed)
	JobQueue* queue = nullptr; //8 bytes
	// Number of current dependencies to other jobs (which this job has to wait for)
	std::atomic<unsigned int> dependencyCount = 0; //should be 4 Bytes (but not guaranteed)
	// Number of dependents of this job
	unsigned int dependentCount = 0; //4 bytes
	// Jobs that depend on this job
	Job* dependents[MAX_DEPENDENT_COUNT] = {}; //8 Bytes * 6 = 48 bytes
	//sum bytes = 8+4+4+4+(8*13)=128bytes, which should be two full cache lines.

	~Job() {
		for (unsigned int i = 0; i < dependentCount; ++i)
		{
			dependents[i]->dependencyCount--;
			dependents[i]->queue->NotifyOne();
		}
	}
};

