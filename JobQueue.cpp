#include "JobQueue.h"
#include "Settings.h"

JobQueue::JobQueue(std::atomic<bool>& isRunning) :isRunning(isRunning) {}

void JobQueue::Push(Job* job)
{
	std::lock_guard<std::mutex> guard(mutex);
	//back of deque is defined as private end
	deque.push_back(job);
	//notify as a job is available
	NotifyOne();
}

Job* JobQueue::Pop()
{
	std::lock_guard<std::mutex> guard(mutex);
	if (deque.empty())
	{
		return nullptr;
	}
	//front is defined as public end, this ensure that workers work on LIFO basis when stealing but use FIFO when working on their own
	//jobs which should be cache friendlier.
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
	//back is defined as private end
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
	//Wait until jobs are available or the system stopped runnning.
	conditionalVariable.wait(lock, [&]()
		{
			return (!isRunning || !deque.empty());
		});
}

void JobQueue::NotifyOne() {
	conditionalVariable.notify_one();
}
