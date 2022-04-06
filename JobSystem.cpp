#include "JobSystem.h"

#include <algorithm>
#include <mutex>
#include <string>
#include "Settings.h"
#include "optick_src/optick.h"

JobSystem::JobSystem(std::atomic<bool>& isRunning) : isRunning(isRunning)
{
	//using max bc hardware_concurrency might return 0 if it cannot read hardware specs 
	//TODO: should we actually use one core less here bc main already uses one?
	//TODO: also make this configurable if the user desires
	const unsigned int core_count = std::max(1u, std::thread::hardware_concurrency());
	//TODO: how many worker are optimal for the core count (for now it just uses 1 to 1)
	for (unsigned int core = 0; core < core_count; ++core)
	{
		PRINT(("CREATING WORKER FOR CORE " + std::to_string(core) + "\n").c_str());
		workers.push_back(std::thread(&JobSystem::Worker, this, core));
	}
}

void JobSystem::JoinJobs()
{
	for (auto& worker : workers)
	{
		worker.join();
	}
}

void JobSystem::CreateJob(void(*jobFunction)())
{
	//TODO: add way to specify dependencies
	Job* job = new Job(jobFunction);
	queue.Push(job);
}

void JobSystem::Worker(unsigned int id)
{
	OPTICK_THREAD(("WORKER #" + std::to_string(id)).c_str());
	while (isRunning)
	{

		PRINT(("WORKER #" + std::to_string(id) + ": WaitForAvailableJobs\n").c_str());
		WaitForAvailableJobs();
		PRINT(("WORKER #" + std::to_string(id) + ": GetJob\n").c_str());
		Job* job = GetJob();
		PRINT(("WORKER #" + std::to_string(id) + ": CanExecuteJob\n").c_str());
		if (CanExecuteJob(job))
		{
			PRINT(("WORKER #" + std::to_string(id) + ": Execute\n").c_str());
			Execute(job);
			PRINT(("WORKER #" + std::to_string(id) + ": Finish\n").c_str());
			Finish(job);
		}
		else
		{
			PRINT(("WORKER #" + std::to_string(id) + ": WorkOnOtherAvailableTask\n").c_str());
			WorkOnOtherAvailableTask();
		}
	}
}

void JobSystem::WaitForAvailableJobs()
{
	//TODO: Wait until Queue has Job 
}

Job* JobSystem::GetJob()
{
	return queue.Pop();
}

bool JobSystem::CanExecuteJob(Job* job)
{
	if (job)
	{
		//TODO: Check for dependencies
		return true;
	}
	return false;
}

void JobSystem::Execute(Job* job)
{
	job->jobFunction();
}

void JobSystem::Finish(Job* job)
{
	//TODO: Mark job as resolved for dependencies and wait for child jobs(?)
}

void JobSystem::WorkOnOtherAvailableTask()
{
	//TODO: not sure, must get new job from queue and put old one back, but how often do we try etc.?
}
