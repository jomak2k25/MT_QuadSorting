#pragma once
#include "pthread.h"
#include <queue>
#include <vector>

// Job structure used to pass work to the ThreadPool
struct JobBase
{
	virtual void DoJob() = 0;
	// Flag to check if the Job has been completed
	bool _Complete = false;
};

struct JobOneParam : public JobBase
{
	virtual void DoJob() override
	{
		_FuncPtr(_Param);
	}
	// Function pointer and arguments
	void(*_FuncPtr)(void*);
	void* _Param;
};

struct JobTwoParams : public JobBase
{
	virtual void DoJob() override
	{
		_FuncPtr(_Param1, _Param2);
	}
	// Function pointer and arguments
	void(*_FuncPtr)(void*, void*);
	void* _Param1;
	void* _Param2;
};

struct JobThreeParams : public JobBase
{
	virtual void DoJob() override
	{
		_FuncPtr(_Param1, _Param2, _Param3);
	}
	// Function pointer and arguments
	void(*_FuncPtr)(void*, void*, void*);
	void* _Param1;
	void* _Param2;
	void* _Param3;
};

// Job Pool allocates and reuses Job Objects which are used to provide work
// to the threadpool
class JobPool
{
public:
	// Default constructor, Jobs will be allocated at a page size of 8
	JobPool()
		:_JobPages_OneParam(), _JobPages_TwoParams(), _JobPages_ThreeParams(), _PageSize(8)
	{}

	// Constructor to specify a custom Job page size
	JobPool(unsigned PageSize)
		:_JobPages_OneParam(), _JobPages_TwoParams(), _JobPages_ThreeParams(), _PageSize(PageSize)
	{}

	// Deconstructor will free all allocated memory for Jobs
	~JobPool()
	{
		for (unsigned i = 0; i < _JobPages_OneParam.size(); ++i)
		{
			free(_JobPages_OneParam[i]);
		}
		for (unsigned i = 0; i < _JobPages_TwoParams.size(); ++i)
		{
			free(_JobPages_TwoParams[i]);
		}
		for (unsigned i = 0; i < _JobPages_ThreeParams.size(); ++i)
		{
			free(_JobPages_ThreeParams[i]);
		}
	}


	// Returns a one parameter job that's free
	JobOneParam* GetFreeJob_OneParam();
	// Returns a two parameter job that's free
	JobTwoParams* GetFreeJob_TwoParams();
	// Returns a three parameter job that's free
	JobThreeParams* GetFreeJob_ThreeParams();

private:

	// Allocated Pages of Job memory

	std::vector<JobOneParam*> _JobPages_OneParam;

	std::vector<JobTwoParams*> _JobPages_TwoParams;

	std::vector<JobThreeParams*> _JobPages_ThreeParams;

	// The size of a Job page for memory allocation
	const unsigned _PageSize;
};

// Thread Pool which can be used to startup multiple threads and feed them jobs to do
class ThreadPool
{
public:
	ThreadPool() = delete;

	// Create a threadpool that will manage the given number of Threads
	ThreadPool(unsigned ThreadCount);

	// Initialises pthread objects and starts up threads
	bool Initialise();

	// Pass a Job to the threadpool to be completed by threads
	void AddWork(JobBase* NewJob);

	// Returns true if all Threads are Idle
	bool AreAllThreadsIdle();

	// Waits for all Threads to become idle and all jobs to be completed
	void WaitForAllThreads();

	// Returns true if threads were successfully killed
	bool StopThreads(bool Safely = true);

	// Get a free
	JobOneParam* GetFreeJob_OneParam();
	JobTwoParams* GetFreeJob_TwoParams();
	JobThreeParams* GetFreeJob_ThreeParams();

	// Gets the number of Jobs completed by threads since initialisation
	long long GetNumJobsCompleted() const;

	// Gets the current number of Idle threads
	unsigned GetNumIdleThreads() const;

private:

	// Static function for Threads to run
	static void* DoWork(void* arg);

	// Counter and mutex to track number of Threads not doing work
	unsigned _IdleThreads;
	pthread_mutex_t _IdleThreads_mutex;


	// Job Queue and accompanying pthread objects to
	//  facilitate providing work to threads
	std::queue<JobBase*> _JobQueue;
	pthread_mutex_t _JobQueue_mutex;
	pthread_cond_t _JobSignaller;

	// Number of threads managed by the pool
	const unsigned _ThreadCount;

	// Vector of pthread handles
	std::vector<pthread_t> _Threads;

	// Bool to signal threads to exit
	bool _EndWork;

	// Job Pool and mutex for allocating Jobs
	JobPool _JobPool;
	pthread_mutex_t _JobPool_mutex;

	// For debugging
	pthread_mutex_t _NumJobsCompleted_mutex;
	long long _NumJobsCompleted;

	// Pthread objects to facilitate waiting for the work to finish
	pthread_cond_t _IdleThreadSignaller;
	pthread_mutex_t _Wait_mutex;

};