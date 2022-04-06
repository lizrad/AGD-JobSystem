#pragma once
#include <queue>
#include <mutex>

struct Job
{
	Job(void(*jobFunction)()) :jobFunction(jobFunction)
	{
	}
	void(*jobFunction)();
	//TODO: still need: data(?), padding to be an exact multiple of cache line size
};

//JobQueue manages thread save access to a queue using a mutex.
class JobQueue
{
	std::queue<Job*> queue;
	std::mutex mutex;
	//TODO: probably also need a conditional variable here to notify when work is available

public:
	void Push(Job* job);
	Job* Pop();
	bool IsEmpty();
};
