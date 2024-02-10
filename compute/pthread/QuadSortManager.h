#pragma once

#include "QuadTree.h"
#include <queue>
#include <string>
#include "ThreadPool.h"

#define __PTW32_STATIC_LIB
#include "pthread.h"

#include "imgui.h"

#include "../../Timer.h"

enum class ThreadingApproach
{
	NoThreading,
	QueueThreading,
	FlatFourThreading,
	ThreadPool
};

class QuadSortManager;

struct FlatFourThreadInfo
{
	int _ThreadID;
	QuadSortManager* _Manager;
};

class QuadSortManager
{
public:
	QuadSortManager() = delete;

	QuadSortManager(unsigned int ThreadCount, ThreadingApproach InitialThreadingApproach,
		Particles* ParticleContainer, size_t QuadCapacity,
		float l, float t, float r, float b);

	~QuadSortManager();

	// Sort Function declaration for any Quad Tree Threading Implementation
	void SortParticles();

	// Worker function for any threaded Quad Tree implementation
	static void* QueueQuadSortWorker(void* inData);

	static void* FlatFourQuadSortWorker(void* inData);

	static void ThreadPoolQuadSort(void* inData, void* inContext);

	// End anything Quad Tree Threading related at end of the program
	void EndQuadTreeThreading();

	void SwapThreadingApproach(ThreadingApproach NewThreadingApproach);

	void ImGuiDraw();
private:
	// Top quad to act as the parent quad for all other quads
	Quad* _TopQuad;

	const unsigned int _ThreadCount;

	// Used to determine which threading approach to use
	ThreadingApproach _CurrentThreadingApproach;

	std::queue<Quad*> _QuadQueue;
	pthread_mutex_t _QuadQueue_mutex;

	QuadPool _QuadPool;
	pthread_mutex_t _QuadPool_mutex;

	ThreadPool _ThreadPool;

	std::vector<pthread_t> _QueueThreads;

	pthread_t _FlatFourThreads[4];
	FlatFourThreadInfo _FlatFourThreadInfos[4];

	// Performance collection data

	Timer<resolutions::milliseconds> _SortPerformanceTimer;

	long long _SortCount = 0;
	long long _SortTime = 0;
	long long _TotalSortTime = 0;
	long long _AvgSortTime = 0;
};
