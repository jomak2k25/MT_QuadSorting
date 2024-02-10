#pragma once
// Vulkan Includes
#include "..\maths.h"             // for standard types
#include "..\utility.h"           // for DBG_ASSERT
#include "..\vulkan_context.h"
#include "..\vulkan_pipeline.h"
#include "..\vulkan_resources.h"

#include <vulkan/vulkan.h>        // for everything vulkan

#include "pthread/Particle.h"

// Imgui includes
#include "imgui.h"

#include "..\Timer.h"

struct compute_UBO_info_buffer
{
	u32 num_elements;
	float right_border;
	float left_border;
	float top_border;
	float bottom_border;
};

struct compute_velocity_push_constants
{
	float x_deltavel;
	float y_deltavel;
};

struct camera_buffer
{
	mat4 vp_matrix;
};
struct model_buffer
{
	mat4 model_matrix;
};

constexpr char const* COMPILED_COMPUTE_SHADER_PATH = "../projects/QuadTreeParticles_pthreads/compute/shaders/vulkan_compute_particles/vulkan_compute_particles.comp.spv";
constexpr char const* COMPILED_GRAPHICS_SHADER_PATH_VERT = "../projects/QuadTreeParticles_pthreads/compute/shaders/vulkan_compute_particles/sprite.vert.spv";
constexpr char const* COMPILED_GRAPHICS_SHADER_PATH_FRAG = "../projects/QuadTreeParticles_pthreads/compute/shaders/vulkan_compute_particles/sprite.frag.spv";


class VulkanParticleMachine
{
public:

	// Delete the default constructor
	VulkanParticleMachine() = delete;

	// Constructor to pass a pointer to the particle data and other app info
	VulkanParticleMachine(Particles* ParticleContainer, float RightBorder, float LeftBorder,
		float TopBorder, float BottomBorder);

	// Call once at the start of the program to setup vulkan resources
	void Initialise();
	
	// Call each frame in the game loop to update and render the particles
	// Should pass in the DeltaTime for this frame
	void Update(float DeltaTime);

	// Call before app closes to release vulkan resources
	void Release();

private:
	
	// Create the Compute Pipeline which moves the particles
	void CreateComputePipeline();

	// Create the Graphics Pipeline which displays the particles on screen
	void CreateGraphicsPipeline();

	// Bind Commands to the compute CmdBuffer and submit to the GPU
	void BindAndSubmitCompute(float DeltaTime);

	// Bind Commands to the graphics CmdBuffer and submit to the GPU
	void BindAndSubmitGraphics();

	// Retrieve updated Particle data from the compute buffers
	void RetrieveParticleData();

	// Release resources created for the graphics pipeline
	void ReleaseGraphicsPipeline();

	// Release resources created for the compute pipeline
	void ReleaseComputePipeline();

	// Pointer to the app's particle container
	Particles* _ParticleContainer;

//---------------------------------------------------------------------------------

	// Vulkan Device objects
	VkPhysicalDevice _PhysicalDevice = VK_NULL_HANDLE;
	VkDevice _Device = VK_NULL_HANDLE;

	// A bit field of flags to request queue types when creating the devices
	VkQueueFlags QueueFlags = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;

	// An array of Semaphores the graphics submission will need to wait for
	// This contains the SwapChainImageAvailable & ComputeComplete semaphores
	VkSemaphore _GraphicsWaitSemaphores[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

	//---------------------------------------------------------------------------------
	// Resources for running the compute pipeline
	
	// Store a prototype of the info to be copied to the UBO info buffer
	compute_UBO_info_buffer _DefaultComputeInfoBuffer;

	// Vulkan Queue for compute
	VkQueue _QueueCompute = VK_NULL_HANDLE;

	static constexpr u32 NUM_SETS_COMPUTE = 1u;

	static constexpr u32 NUM_RESOURCES_COMPUTE_SET_0 = 3u;
	static constexpr u32 BINDING_ID_SET_0_SBO_XPOS = 0u;
	static constexpr u32 BINDING_ID_SET_0_SBO_YPOS = 1u;
	static constexpr u32 BINDING_ID_SET_0_UBO_INFO = 2u;

	std::array <VkDescriptorSetLayout, NUM_SETS_COMPUTE> _DescriptorSetLayoutsCompute = { VK_NULL_HANDLE };
	VkPipelineLayout _PipelineLayoutCompute = VK_NULL_HANDLE;
	VkPipeline _PipelineCompute = VK_NULL_HANDLE;

	VkDescriptorPool _DescriptorPoolCompute = VK_NULL_HANDLE;
	vulkan_descriptor_set _DescSet0Compute; // for buffer_XPos + buffer_YPos + buffer_info

	vulkan_buffer _BufferXPos, _BufferYPos, _BufferInfo;

	VkCommandPool _CommandPoolCompute = VK_NULL_HANDLE;
	VkCommandBuffer _CommandBufferCompute = VK_NULL_HANDLE;

	// Fence used to signal when the compute pipeline has finished executing
	VkFence _FenceCompute = VK_NULL_HANDLE;
	VkSemaphore& _SemaphoreComputeComplete = _GraphicsWaitSemaphores[0];

	//---------------------------------------------------------------------------------
	// Resources for running the graphics pipeline

	// Vulkan Queue for graphics
	VkQueue _QueueGraphics = VK_NULL_HANDLE;

	// Swapchain resources
	VkExtent2D _SwapChainExtent = {};
	VkRenderPass _RenderPass = {};

	static constexpr u32 NUM_SETS_GRAPHICS = 2u;

	static constexpr u32 NUM_RESOURCES_GRAPHICS_SET_0 = 2u;
	static constexpr u32 BINDING_ID_SET_0_UBO_CAMERA = 0u;
	static constexpr u32 BINDING_ID_SET_0_UBO_MODEL = 1u;

	static constexpr u32 NUM_RESOURCES_GRAPHICS_SET_1 = 1u;
	static constexpr u32 BINDING_ID_SET_1_COLOUR = 0u;

	std::array <VkDescriptorSetLayout, NUM_SETS_GRAPHICS> _DescriptorSetLayoutsGraphics = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkPipelineLayout _PipelineLayoutGraphics = VK_NULL_HANDLE;
	VkPipeline _PipelineGraphics = VK_NULL_HANDLE;

	VkDescriptorPool _DescriptorPoolGraphics = VK_NULL_HANDLE;
	vulkan_descriptor_set _DescSet0Graphics,  // for rendering input image , BufferGraphicsCamera + BufferGraphicsModel
		_DescSet1Graphics;  // Used to pass a colour for the particles to the fragment shader

	vulkan_buffer _BufferGraphicsCamera,
		_BufferGraphicsModel,
		_BufferGraphicsColour;

	VkCommandPool _CommandPoolGraphics = VK_NULL_HANDLE;
	VkCommandBuffer _CommandBufferGraphics = VK_NULL_HANDLE;

	vulkan_mesh _MeshSprite;

	VkSemaphore& _SwapchainImageAvailableSemaphore = _GraphicsWaitSemaphores[1];
	VkFence _FenceSubmitGraphics = VK_NULL_HANDLE;

	// ImGui Functionality & Performance tracking
public:
	// Allow something to set a callback function for drawing with imgui
	void SetImGuiCallback(void(*DrawCallback)());

private:
	// Initialise Imgui for vulkan and GLFW
	void InitialiseImGui();
	// Update imgui each frame to start a new frame and draw
	void UpdateImGui();

private:
	// Descriptor pool for imgui
	VkDescriptorPool _ImGuiPool = VK_NULL_HANDLE;

	// Function pointer for a potential imgui drawing callback function
	void(*_DrawCallback)() = nullptr;

	// Bool to determine if the compute and graphics updated should be executed without overlap
	// and timed seperately
	bool _SeparateUpdate = false;

	// Counter for how many frames have been timed with compute and graphics combined
	long long _UpdateCount = 0;

	// Update Timer and time to record the latest time it took to execute compute and graphics combined
	Timer<resolutions::microseconds> _UpdateTimer;
	long long _UpdateTime = 0;

	// Running time total to calculate the average combined updated time
	long long _UpdateTimeTotal = 0;

	// Counter for how many frames have been timed with compute and graphics seperated
	long long _SeparateUpdateCount = 0;

	// Update timer and time to record the latest time it took to execute compute while seperated
	Timer<resolutions::microseconds> _ComputeUpdateTimer;
	long long _ComputeUpdateTime = 0;

	// Running time total to calculate the average compute time
	long long _ComputeUpdateTimeTotal = 0;

	// Update timer and time to record the latest time it took to execute graphics while seperated
	Timer<resolutions::microseconds> _GraphicsUpdateTimer;
	long long _GraphicsUpdateTime = 0;

	// Running time total to calculate the average graphics time
	long long _GraphicsUpdateTimeTotal = 0;

};