#include "JobQueue.h"
#include "Settings.h"

JobQueue::JobQueue(std::atomic<bool>& isRunning) :isRunning(isRunning) {}

void JobQueue::Push(Job* job)
{
	std::lock_guard<std::mutex> guard(queueMutex);
	deque.push_front(job);
	NotifyOne();
}

Job* JobQueue::Pop()
{
	std::lock_guard<std::mutex> guard(queueMutex);
	if (deque.empty())
	{
		return nullptr;
	}
	Job* job = deque.front();
	deque.pop_front();
	return job;
}

Job* JobQueue::Steal()
{
	std::lock_guard<std::mutex> guard(queueMutex);
	if (deque.empty())
	{
		return nullptr;
	}
	Job* job = deque.back();
	deque.pop_back();
	return job;
}

void JobQueue::WaitForJob() {
	std::mutex mutex;
	std::unique_lock<std::mutex> lock(mutex);
	queueConditionalVariable.wait(lock, [&]()
		{
			return !isRunning || !deque.empty();
		});
}

void JobQueue::NotifyAll() {
	queueConditionalVariable.notify_all();
}

void JobQueue::NotifyOne() {
	queueConditionalVariable.notify_one();
}
