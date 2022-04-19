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
	//Push job onto public end of the queue
	void Push(Job* job);
	//Pop a job from the private end of the queue
	Job* Pop();
	//Pop a job from the public end of the queue
	Job* Steal();
	bool IsEmpty();
	//Wait until the queue is not empty anymore
	void WaitForJob();
	//Notify someone waiting on the queue to not be empty anymore
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
	//Pointer to the queue the job is stored in. Used to notify when job becomes workable.
	JobQueue* queue = nullptr; //8 bytes
	// Number of current dependencies to other jobs (which this job has to wait for)
	std::atomic<unsigned int> dependencyCount = 0; //should be 4 Bytes (but not guaranteed)
	// Number of dependents of this job
	unsigned int dependentCount = 0; //4 bytes
	// Jobs that depend on this job
	Job* dependents[MAX_DEPENDENT_COUNT] = {}; //8 Bytes * 6 = 48 bytes
	//Sum bytes = 8+4+4+4+(8*13)=128bytes, which should be two full cache lines.

	~Job() {
		for (unsigned int i = 0; i < dependentCount; ++i)
		{
			//Job is finished, so depentens can reduce dependencyCount
			dependents[i]->dependencyCount--;
			if (dependents[i]->dependencyCount == 0) {
				//This check is necessary because depencies could be finished before dependents even get added to any queue
				if (dependents[i]->queue) {
					//If dependent is workable notify the queue
					dependents[i]->queue->NotifyOne();
				}
			}
		}
	}
};

