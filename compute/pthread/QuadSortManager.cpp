#include "QuadSortManager.h"
#include "../../Logging.h"
#include "imgui.h"


QuadSortManager::QuadSortManager(unsigned int ThreadCount, ThreadingApproach InitialThreadingApproach,
    Particles* ParticleContainer, size_t QuadCapacity,
    float l, float t, float r, float b)
    : _TopQuad(nullptr), _ThreadCount(ThreadCount), _CurrentThreadingApproach(InitialThreadingApproach),
    _QuadQueue_mutex(), _QuadPool_mutex(), _ThreadPool(ThreadCount), _QueueThreads(ThreadCount),
    _FlatFourThreads()
{
    _TopQuad = new Quad(ParticleContainer, nullptr, &_QuadPool, QuadCapacity, l,
        t, r, b, true);

    pthread_mutex_init(&_QuadPool_mutex, NULL);
    pthread_mutex_init(&_QuadQueue_mutex, NULL);

    if (_CurrentThreadingApproach == ThreadingApproach::FlatFourThreading)
    {
        for (int i = 0; i < 4; ++i)
        {
            _FlatFourThreadInfos[i]._ThreadID = i;
            _FlatFourThreadInfos[i]._Manager = this;
        }
        
    }


    if(_CurrentThreadingApproach == ThreadingApproach::ThreadPool)
    {
        if (!_ThreadPool.Initialise())
        {
            DBG_LOG_ASSERT(false, "QuadTreeThreading.cpp", "Failed to initialise threadpool");
        }
    }
}

QuadSortManager::~QuadSortManager()
{
    delete _TopQuad;
    if (_CurrentThreadingApproach == ThreadingApproach::ThreadPool)
    {
        _ThreadPool.StopThreads(false);
    }
}

// Sort Function declaration for any Quad Tree Threading Implementation
void QuadSortManager::SortParticles()
{
    // Reset sort performance timer
    _SortPerformanceTimer.restart();

    // Reset the quad pool to make all previosly allocated quads available
    _QuadPool.Reset();

    // Sort Particles based on the currently selected approach
    if (_CurrentThreadingApproach == ThreadingApproach::NoThreading)
    {
        // Check the Top Quad should be broken before pushing it to the quad queue
        if (_TopQuad->ShouldBreak())
        {
            _QuadQueue.push(_TopQuad);
        }

        // Loop until all Quads from the queue have been sorted
        for (; !_QuadQueue.empty();)
        {
            // Get the quad from the front of the queue
            Quad* CurrentQuad = _QuadQueue.front();
            _QuadQueue.pop();

            // Allocate child quads and then sort the quad
            CurrentQuad->AllocateChildQuads();
            CurrentQuad->SortChildQuads();

            // Iterate through the 4 child quads
            for (unsigned i = 0; i < 4; ++i)
            {
                // Only add child quads to the queue if they should be broken
                if ((CurrentQuad->_ChildQuads + i)->ShouldBreak())
                {
                    _QuadQueue.push(CurrentQuad->_ChildQuads + i);
                }
            }
        }
    }
    else if (_CurrentThreadingApproach == ThreadingApproach::QueueThreading)
    {
        // Check the Top Quad should be broken before pushing it to the quad queue
        if (_TopQuad->ShouldBreak())
        {
            // Break initial quad
            _TopQuad->AllocateChildQuads();
            _TopQuad->SortChildQuads();
            for (unsigned i = 0; i < 4; ++i)
            {
                if ((_TopQuad->_ChildQuads + i)->ShouldBreak())
                {
                    _QuadQueue.push(_TopQuad->_ChildQuads + i);
                }
            }

            // Start up threads on sorting the quad tree
            for (int i = 0; i < _ThreadCount; ++i)
            {
                if (pthread_create(&_QueueThreads[i], NULL, &QueueQuadSortWorker, this))
                {
                    DBG_LOG_ERROR("QuadSortManager.cpp", "Failed to create threads in Basic Queue Threading Approach!");
                }
            }

            // Wait for threads to finish
            for (int i = 0; i < _ThreadCount; ++i)
            {
                if (pthread_join(_QueueThreads[i], NULL)) 
                {
                    DBG_LOG_ERROR("QuadSortManager.cpp", "Failed to join threads in Basic Queue Threading Approach!");
                }
            }
        }
    }
    else if (_CurrentThreadingApproach == ThreadingApproach::FlatFourThreading)
    {
        if (_TopQuad->ShouldBreak())
        {
            pthread_mutex_init(&_QuadPool_mutex, NULL);

            // Break initial quad
            pthread_t threads[4];

            // Sort the top level quad into 4 child quads
            _TopQuad->AllocateChildQuads();
            _TopQuad->SortChildQuads();

            // Start threads on sorting a quad each
            for (unsigned i = 0; i < 4; ++i)
            {
                if ((_TopQuad->_ChildQuads + i)->ShouldBreak())
                {
                    if (pthread_create(&_FlatFourThreads[i], NULL, &FlatFourQuadSortWorker, _FlatFourThreadInfos+i))
                    {
                        DBG_LOG_ERROR("QuadSortManager.cpp", "Failed to create threads in Flat Four approach!");
                    }
                }
            }

            // Wait for threads to finish
            for (unsigned i = 0; i < 4; ++i)
            {
                if (pthread_join(_FlatFourThreads[i], NULL)) 
                {
                    DBG_LOG_ERROR("QuadSortManager.cpp", "Failed to join threads in Flat Four approach!"); 
                }
            }
        }
    }
    else if(_CurrentThreadingApproach == ThreadingApproach::ThreadPool)
    {
        ThreadPoolQuadSort(_TopQuad, &_ThreadPool);

        // Wait for threads to finish
        _ThreadPool.WaitForAllThreads();

    }

    // Collect performance information
    ++_SortCount;

    _SortTime = _SortPerformanceTimer.total_elapsed();

    _TotalSortTime += _SortTime;
    _AvgSortTime = _TotalSortTime / _SortCount;
}

// Worker function for any threaded Quad Tree implementation
void* QuadSortManager::QueueQuadSortWorker(void* inData)
{
    QuadSortManager* pQuadSortManager = (QuadSortManager*)inData;

    bool Continue = true;
    Quad* CurrentQuad = nullptr;

    for (; Continue;)
    {
        pthread_mutex_lock(&pQuadSortManager->_QuadQueue_mutex);

        Continue = !pQuadSortManager->_QuadQueue.empty();

        if (Continue)
        {
            CurrentQuad = pQuadSortManager->_QuadQueue.front();
            pQuadSortManager->_QuadQueue.pop();
        }

        pthread_mutex_unlock(&pQuadSortManager->_QuadQueue_mutex);

        if (Continue)
        {
            pthread_mutex_lock(&pQuadSortManager->_QuadPool_mutex);

            if (!CurrentQuad->AllocateChildQuads())
            {
                pthread_mutex_unlock(&pQuadSortManager->_QuadPool_mutex);
                DBG_LOG_ERROR("QuadSortManager.cpp", "Failed to allocate quads");
                return nullptr;
            }

            pthread_mutex_unlock(&pQuadSortManager->_QuadPool_mutex);

            CurrentQuad->SortChildQuads();

            for (unsigned i = 0; i < 4; ++i)
            {
                if ((CurrentQuad->_ChildQuads + i)->ShouldBreak())
                {
                    pthread_mutex_lock(&pQuadSortManager->_QuadQueue_mutex);
                    pQuadSortManager->_QuadQueue.push(CurrentQuad->_ChildQuads + i);
                    pthread_mutex_unlock(&pQuadSortManager->_QuadQueue_mutex);
                }
            }
        }
    }
    return nullptr;
}

// Worker function for any threaded Quad Tree implementation
void* QuadSortManager::FlatFourQuadSortWorker(void* inData)
{
    FlatFourThreadInfo* ThreadInfo = (FlatFourThreadInfo*)inData;

    QuadSortManager* ThisManager = ThreadInfo->_Manager;

    // Get the quad this thread should start with from the manager
    Quad* StartQuad = ThisManager->_TopQuad->_ChildQuads + ThreadInfo->_ThreadID;

    std::queue<Quad*> QuadQueue;
    QuadQueue.push(StartQuad);

    for (; !QuadQueue.empty();)
    {
        Quad* CurrentQuad = QuadQueue.front();
        QuadQueue.pop();

        pthread_mutex_lock(&ThisManager->_QuadPool_mutex);

        if (!CurrentQuad->AllocateChildQuads())
        {
            pthread_mutex_unlock(&ThisManager->_QuadPool_mutex);
            DBG_LOG_ERROR("QuadSortManager.cpp", "Failed to allocate quads");
            return nullptr;
        }

        pthread_mutex_unlock(&ThisManager->_QuadPool_mutex);

        CurrentQuad->SortChildQuads();

        for (unsigned i = 0; i < 4; ++i)
        {
            if ((CurrentQuad->_ChildQuads + i)->ShouldBreak())
            {
                QuadQueue.push(CurrentQuad->_ChildQuads + i);
            }
        }
    }
    return nullptr;
}

// Worker function for any threaded Quad Tree implementation
void QuadSortManager::ThreadPoolQuadSort(void* inData, void* inContext)
{
    //QuadSortWorkerData*
    Quad* CurrentQuad = (Quad*)inData;
    QuadSortManager* ThisManager = (QuadSortManager*)inContext;

    pthread_mutex_lock(&ThisManager->_QuadPool_mutex);

    if (!CurrentQuad->AllocateChildQuads())
    {
        DBG_LOG_ERROR("QuadTreeThreading.cpp", "Failed to allocate quads!");
    }

    pthread_mutex_unlock(&ThisManager->_QuadPool_mutex);

    CurrentQuad->SortChildQuads();

    for (unsigned i = 0; i < 4; ++i)
    {
        if ((CurrentQuad->_ChildQuads + i)->ShouldBreak())
        {
            // Acquire an empty job from the thread pool
            JobTwoParams* NewJob = ThisManager->_ThreadPool.GetFreeJob_TwoParams();
            // Fill in the Job data
            NewJob->_FuncPtr = &ThreadPoolQuadSort;
            NewJob->_Param1 = CurrentQuad->_ChildQuads + i;
            NewJob->_Param2 = ThisManager;

            // Pass the Job to the threadpool
            ThisManager->_ThreadPool.AddWork(NewJob);
        }
    }
}


// End anything Quad Tree Threading related at end of the program
void QuadSortManager::EndQuadTreeThreading()
{
    if (_CurrentThreadingApproach == ThreadingApproach::ThreadPool)
    {
        _ThreadPool.StopThreads(false);
    }
}

void QuadSortManager::SwapThreadingApproach(ThreadingApproach NewThreadingApproach)
{
    if (NewThreadingApproach == _CurrentThreadingApproach)
    {
        return;
    }

    // Safely eject from using a Thread Pool
    if (_CurrentThreadingApproach == ThreadingApproach::ThreadPool)
    {
        _ThreadPool.StopThreads(true);
    }

    // Set the current threading approach
    _CurrentThreadingApproach = NewThreadingApproach;

    // Initialise new threading approach if needed

    if (_CurrentThreadingApproach == ThreadingApproach::ThreadPool)
    {
        if (!_ThreadPool.Initialise())
        {
            DBG_LOG_ASSERT(false, "QuadSortManager.cpp", "Thread Pool failed to initialise!");
        }
    }

}

void QuadSortManager::ImGuiDraw()
{
    ImGui::Text("Sort Performance:");
    ImGui::Text("Total Sort Time(ms): %lli", _TotalSortTime);
    ImGui::Text("Sort Time(ms): %lli", _SortTime);
    ImGui::Text("Average Sort Time(ms): %lli", _AvgSortTime);
    unsigned int AllocatedQuadCount = (_QuadPool._End + 1) * _QuadPool._PageSize;
    ImGui::Text("Quad Count = %d \n", AllocatedQuadCount);
    ImGui::NewLine();

    if (_CurrentThreadingApproach == ThreadingApproach::ThreadPool)
    {
        ImGui::Text("Num Jobs Completed: %lli", _ThreadPool.GetNumJobsCompleted());
        ImGui::Text("Num Idle Threads: %d", _ThreadPool.GetNumIdleThreads());
    }
}