#include "JobQueue.h"
#include "Settings.h"

JobQueue::JobQueue(std::atomic<bool>& isRunning) :isRunning(isRunning) {}

void JobQueue::Push(Job* job)
{
	std::lock_guard<std::mutex> guard(mutex);
	deque.push_back(job);
	NotifyOne();
}

Job* JobQueue::Pop()
{
	std::lock_guard<std::mutex> guard(mutex);
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
	std::lock_guard<std::mutex> guard(mutex);
	if (deque.empty())
	{
		return nullptr;
	}
	Job* job = deque.back();
	deque.pop_back();
	return job;
}
bool JobQueue::IsEmpty() {
	std::lock_guard<std::mutex> guard(mutex);
	return deque.empty();
}
void JobQueue::WaitForJob() {
	std::mutex tempMutex;
	std::unique_lock<std::mutex> lock(tempMutex);
	conditionalVariable.wait(lock, [&]()
		{
			return (!isRunning || !deque.empty());
		});
}

void JobQueue::NotifyOne() {
	conditionalVariable.notify_one();
}
