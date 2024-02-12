#include "ThreadPool.h"

ThreadPool::ThreadPool(unsigned ThreadCount)
	:_IdleThreads(0),_JobQueue(), _ThreadCount(ThreadCount), _Threads(ThreadCount), _EndWork(false), _NumJobsCompleted(0)
{
	pthread_mutex_init(&_IdleThreads_mutex, NULL);
	pthread_mutex_init(&_JobQueue_mutex, NULL);
	pthread_mutex_init(&_JobPool_mutex, NULL);
	pthread_mutex_init(&_NumJobsCompleted_mutex, NULL);

	// Initialise conditional variables
	pthread_cond_init(&_JobSignaller, NULL);
}

bool ThreadPool::Initialise()
{
	for (unsigned i = 0; i < _ThreadCount; ++i)
	{
		_Threads.push_back(pthread_t());

		if (pthread_create(&_Threads[i], NULL, &this->DoWork, this)) { return false; }
	}
	return true;
}

void ThreadPool::AddWork(JobBase* NewJob)
{
	// Acquire mutex lock and add jobs to the queue
	pthread_mutex_lock(&_JobQueue_mutex);

	_JobQueue.push(NewJob);

	pthread_mutex_unlock(&_JobQueue_mutex);

	// Signal the threads that work has been added
	pthread_cond_signal(&_JobSignaller);
}

bool ThreadPool::AreAllThreadsIdle()
{
	pthread_mutex_lock(&_JobQueue_mutex);
	pthread_mutex_lock(&_IdleThreads_mutex);
	bool AllThreadsIdle(_IdleThreads == _ThreadCount && _JobQueue.empty());
	pthread_mutex_unlock(&_JobQueue_mutex);
	pthread_mutex_unlock(&_IdleThreads_mutex);
	return AllThreadsIdle;
}

void ThreadPool::WaitForAllThreads(bool WaitForWorkStart)
{
	if (WaitForWorkStart)
	{
		while (!_WorkStarted)
		{}
	}
	while (!AreAllThreadsIdle())
	{}
	_WorkStarted = false;
}

bool ThreadPool::StopThreads(bool Safely)
{
	if (Safely)
	{
		// Check All threads are idle
		if (!AreAllThreadsIdle())
		{
			// Flush the Job Queue of any outstanding jobs
			pthread_mutex_lock(&_JobQueue_mutex);

			while (!_JobQueue.empty())
			{
				_JobQueue.front()->_Complete = true;
				_JobQueue.pop();
			}

			pthread_mutex_unlock(&_JobQueue_mutex);

			// Wait for all threads to become idle
			while (!AreAllThreadsIdle()) {}
		}

		_EndWork = true;
		pthread_cond_signal(&_JobSignaller);

		for (int i = 0; i < _Threads.size(); ++i)
		{
			if (pthread_join(_Threads[i], NULL)) { return false; };
		}
	}
	else
	{
		for (int i = 0; i < _Threads.size(); ++i)
		{
			pthread_kill(_Threads[i], NULL);
		}
	}
	_Threads.clear();

	return true;
}

template<class JobType>
static JobType* GetFreeJob(std::vector<JobType*>& JobPages, const unsigned PageSize)
{
	// Find a completed Job to reuse
	for (unsigned i = 0; i < JobPages.size(); ++i)
	{
		for (unsigned j = 0; j < PageSize; ++j)
		{
			if ((JobPages[i] + j)->_Complete)
			{
				(JobPages[i] + j)->_Complete = false;
				return (JobPages[i] + j);
			}
		}
	}

	// Allocate a new page of jobs
	JobType* NewPage = new JobType[PageSize];
	JobPages.push_back(NewPage);
	for (unsigned i = 1; i < PageSize; ++i)
	{
		(JobPages[JobPages.size() - 1] + i)->_Complete = true;
	}
	// Return a new job one cannot be reused
	return (JobPages[JobPages.size() - 1]);
}

JobOneParam* JobPool::GetFreeJob_OneParam()
{
	return GetFreeJob(_JobPages_OneParam, _PageSize);
}

JobTwoParams* JobPool::GetFreeJob_TwoParams()
{
	return GetFreeJob(_JobPages_TwoParams, _PageSize);
}

JobThreeParams* JobPool::GetFreeJob_ThreeParams()
{
	return GetFreeJob(_JobPages_ThreeParams, _PageSize);
}

// Get a free Job that handles one paramter
JobOneParam* ThreadPool::GetFreeJob_OneParam()
{
	pthread_mutex_lock(&_JobPool_mutex);
	JobOneParam* ret(_JobPool.GetFreeJob_OneParam());
	pthread_mutex_unlock(&_JobPool_mutex);
	return ret;
}
JobTwoParams* ThreadPool::GetFreeJob_TwoParams()
{
	pthread_mutex_lock(&_JobPool_mutex);
	JobTwoParams* ret(_JobPool.GetFreeJob_TwoParams());
	pthread_mutex_unlock(&_JobPool_mutex);
	return ret;
}
JobThreeParams* ThreadPool::GetFreeJob_ThreeParams()
{
	pthread_mutex_lock(&_JobPool_mutex);
	JobThreeParams* ret(_JobPool.GetFreeJob_ThreeParams());
	pthread_mutex_unlock(&_JobPool_mutex);
	return ret;
}

long long ThreadPool::GetNumJobsCompleted() const
{
	return _NumJobsCompleted;
}

unsigned ThreadPool::GetNumIdleThreads() const
{
	return _IdleThreads;
}

void* ThreadPool::DoWork(void* arg)
{
	ThreadPool* ThisPool = (ThreadPool*)arg;

	bool QueueEmpty(true);
	JobBase* CurrentJob(nullptr);

	while (ThisPool->_EndWork == false)
	{
		// Check the job queue for work
		pthread_mutex_lock(&ThisPool->_JobQueue_mutex);

		QueueEmpty = ThisPool->_JobQueue.empty();

		if (!QueueEmpty)
		{
			CurrentJob = ThisPool->_JobQueue.front();
			ThisPool->_JobQueue.pop();
		}

		pthread_mutex_unlock(&ThisPool->_JobQueue_mutex);

		if (QueueEmpty)
		{
			pthread_mutex_lock(&ThisPool->_IdleThreads_mutex);
			++ThisPool->_IdleThreads;
			pthread_cond_wait(&ThisPool->_JobSignaller, &ThisPool->_IdleThreads_mutex);
			--ThisPool->_IdleThreads;
			pthread_mutex_unlock(&ThisPool->_IdleThreads_mutex);
		}
		else
		{
			CurrentJob->DoJob();
			CurrentJob->_Complete = true;
			pthread_mutex_lock(&ThisPool->_NumJobsCompleted_mutex);
			++ThisPool->_NumJobsCompleted;
			pthread_mutex_unlock(&ThisPool->_NumJobsCompleted_mutex);
			ThisPool->_WorkStarted = true;
		}
	}
	return nullptr;
}