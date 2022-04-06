#include "JobQueue.h"

void JobQueue::Push(Job* job)
{
	std::lock_guard<std::mutex> guard(mutex);
	queue.push(job);
}

Job* JobQueue::Pop()
{
	std::lock_guard<std::mutex> guard(mutex);
	if (queue.empty())
	{
		return nullptr;
	}
	Job* job = queue.front();
	queue.pop();
	return job;
}

bool JobQueue::IsEmpty()
{
	std::lock_guard<std::mutex> guard(mutex);
	return queue.empty();
}
