// Main.cpp
// Program creates particles with random locations and then sorts them into a quadtree

// Windows Things
#define VK_USE_PLATFORM_WIN32_KHR // tell vulkan we are on Windows platform
#include <Windows.h>
#define WIN32_LEAN_AND_MEAN       // exclude rarely-used content from the Windows headers
#define NOMINMAX                  // prevent Windows macros defining their own min and max macros

//#include <cstdio>
#include "Timer.h"
#include <queue>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

// Include the class used to update and render particles through vulkan
#include "compute/VulkanParticleMachine.h"

// Include the quad sort manager to handle quad sorting implementations
#include "compute/pthread/QuadSortManager.h"

// Number of threads the Quad Sort Manager can use for Pool and Queue managed threading approached
static constexpr unsigned int SORT_THREAD_COUNT = 4;

// Global Constants
static constexpr unsigned NUM_PARTICLES = 256u;
static constexpr float PARTICLE_RADIUS = 1.f;
static constexpr float PARTICLE_X_VEL = -18.f;
static constexpr float PARTICLE_Y_VEL = -25.f;

static constexpr unsigned SCREEN_WIDTH = 1920u;
static constexpr unsigned SCREEN_HEIGHT = 1080u;
// Changed for texture rendering
static constexpr int WORLD_RIGHT = SCREEN_WIDTH; // Changed for texture rendering
static constexpr int WORLD_LEFT = 0;
static constexpr int WORLD_TOP = 0;
static constexpr int WORLD_BOTTOM = SCREEN_HEIGHT;

//Debug Performance variables
Timer<resolutions::milliseconds> FrameTimer;
Timer<resolutions::milliseconds> MachineUpdateTimer;

// Globals for storing and displaying frame performance info
long long FrameCount(0);
long long FrameTime(0);
long long AvgFrameTime(0);
long long TotalElapsedTime(0);

// Global variable for tracking how long it took for the Vulkan Particle machine to update
long long MachineUpdateTime(0);

// The soft capacity of each quad in the quad tree
static constexpr size_t QUAD_CAPACITY = 4u;

bool EnableQuadSorting = true;

static QuadSortManager* SortManager = nullptr;

void DrawImGuiWindow()
{
    if (ImGui::Begin("QuadTreeParticles Info"))
    {
        // Provide option to overlay quads for debugging
        ImGui::Checkbox("Enable Quad Sorting", &EnableQuadSorting);

        ImGui::Text("Particle count: %d \n", NUM_PARTICLES);

        // Display Performance info
        ImGui::Text("Performance:");
        ImGui::Text("Frame Count: %lli", FrameCount);
        ImGui::Text("Total Elapsed Time(ms): %lli", TotalElapsedTime);
        ImGui::Text("Frame Time(ms): %lli", FrameTime);
        ImGui::Text("Average Frame Time(ms): %lli", AvgFrameTime);
        ImGui::NewLine();
        // Display performance info on sorting if it is enabled
        if (EnableQuadSorting && SortManager)
        {
            SortManager->ImGuiDraw();
        }
        ImGui::End();
    }
}


int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int  nShowCmd)
{
    // Create a window with GLFW
    create_window("QuadTree Particles Vulkan", SCREEN_WIDTH, SCREEN_HEIGHT);

    // Seed Rand
    srand(time(NULL));

    // Create a container of particles and randomise their locations
    Particles ParticleContainer(NUM_PARTICLES, PARTICLE_RADIUS, PARTICLE_X_VEL, PARTICLE_Y_VEL);
    // Randomise start locations of particles across world space
    ParticleContainer.RandomiseLocationsInRange(WORLD_LEFT, WORLD_RIGHT, WORLD_TOP, WORLD_BOTTOM);

    SortManager = new QuadSortManager(SORT_THREAD_COUNT, ThreadingApproach::ThreadPool, &ParticleContainer, QUAD_CAPACITY,
        static_cast<float>(WORLD_LEFT), static_cast<float>(WORLD_TOP), static_cast<float>(WORLD_RIGHT), static_cast<float>(WORLD_BOTTOM));

    // Create a Vulkan Based Particle Machine
    VulkanParticleMachine ParticleMachine(&ParticleContainer, WORLD_RIGHT, WORLD_LEFT, WORLD_TOP, WORLD_BOTTOM);

    // Initialise the Partice Machine to setup vulkan etc..
    ParticleMachine.Initialise();

    // Set a callback function from within the Vulkan Particle Machine for ImGui Draw calls
    ParticleMachine.SetImGuiCallback(&DrawImGuiWindow);

    // Bool flags for rendering debug information
    bool DrawDebugQuads = false;

    // Initialise Delta Timer to 0
    float DeltaTime = 0.f;

    // Run the main update loop
    FrameTimer.restart();
    while(process_os_messages()) // Check for window closure here
    {
        ++FrameCount;
        
        if (EnableQuadSorting)
        {
            // Call to sort the particles with the defined implementation
            SortManager->SortParticles();
        }

        // Update Performance times
        FrameTime = FrameTimer.delta_elapsed();
        TotalElapsedTime = FrameTimer.total_elapsed();
        AvgFrameTime = TotalElapsedTime / FrameCount;

        // Update time to this frame time in seconds
        DeltaTime = (float)FrameTime / 1000.f;

        // Update and render with the particle machine
        ParticleMachine.Update(DeltaTime);
    }

    // Release the GLFW Window
    release_window();

    // Release the Vulkan resources within the particle machine
    ParticleMachine.Release();

    delete SortManager;

    return 0;
}