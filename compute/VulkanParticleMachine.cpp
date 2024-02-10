#include "VulkanParticleMachine.h"


#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"


VulkanParticleMachine::VulkanParticleMachine(Particles* ParticleContainer, float RightBorder, float LeftBorder,
    float TopBorder, float BottomBorder)
    :_ParticleContainer(ParticleContainer), 
    _DefaultComputeInfoBuffer{(u32)_ParticleContainer->_MaxParticles, RightBorder, LeftBorder, TopBorder, BottomBorder}
{}

void VulkanParticleMachine::Initialise()
{
    // create vulkan instance, device etc
    // one per app
    {
        if (!create_vulkan_instance())
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_surface())
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_device(QueueFlags,
            _PhysicalDevice, _Device))
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_swapchain(_SwapChainExtent, _RenderPass))
        {
            DBG_ASSERT(false);
        }
    }

    // get vulkan queues
    // once per app
    {
        if (!get_vulkan_queue_compute(_QueueCompute))
        {
            DBG_ASSERT(false);
        }

        if (!get_vulkan_queue_graphics(_QueueGraphics))
        {
            DBG_ASSERT(false);
        }
    }

    CreateComputePipeline();

    CreateGraphicsPipeline();
   
    InitialiseImGui();

}

void VulkanParticleMachine::CreateComputePipeline()
{
    {
        // describe the descriptors in set 0
        std::array <VkDescriptorSetLayoutBinding, NUM_RESOURCES_COMPUTE_SET_0> const compute_descriptor_set_layout_binding_info_0 =
        {
          VkDescriptorSetLayoutBinding {
            .binding = BINDING_ID_SET_0_SBO_XPOS,             // at binding point 0 we have
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // an SBO (input/output)
            .descriptorCount = 1u,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = VK_NULL_HANDLE
          },
        VkDescriptorSetLayoutBinding {
            .binding = BINDING_ID_SET_0_SBO_YPOS,             // at binding point 1 we have
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // an SBO (input/output)
            .descriptorCount = 1u,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = VK_NULL_HANDLE
          },
        VkDescriptorSetLayoutBinding {
            .binding = BINDING_ID_SET_0_UBO_INFO,             // at binding point 2 we have
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // a UBO (info)
            .descriptorCount = 1u,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = VK_NULL_HANDLE
          }
        };

        // group up all descriptor layout descriptions
        std::array <std::span <const VkDescriptorSetLayoutBinding>, NUM_SETS_COMPUTE> const compute_descriptor_set_layout_bindings =
        {
          compute_descriptor_set_layout_binding_info_0 // set 0                         
        };

        if (!create_vulkan_descriptor_set_layouts(_Device,
            NUM_SETS_COMPUTE,
            compute_descriptor_set_layout_bindings.data(),
            _DescriptorSetLayoutsCompute.data()))
        {
            DBG_ASSERT(false);
        }

        // describe push constants structure
        VkPushConstantRange const push_constant_ranges[] =
        {
          {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0u,
            .size = sizeof(compute_velocity_push_constants)
          }
        };

        if (!create_vulkan_pipeline_layout(_Device,
            _DescriptorSetLayoutsCompute.size(), _DescriptorSetLayoutsCompute.data(),
            1u, push_constant_ranges,
            _PipelineLayoutCompute))
        {
            DBG_ASSERT(false);
        }

        VkShaderModule shader_module_compute = VK_NULL_HANDLE;
        if (!create_vulkan_shader(_Device,
            COMPILED_COMPUTE_SHADER_PATH,
            shader_module_compute))
        {
            DBG_ASSERT(false);
        }

        if (!create_vulkan_pipeline_compute(_Device,
            shader_module_compute, "main",
            _PipelineLayoutCompute,
            _PipelineCompute))
        {
            DBG_ASSERT(false);
        }

        release_vulkan_shader(_Device,
            shader_module_compute); // don't need shader object now we have the pipeline
    }

    // at least one per pipeline...
    {
        // create pool of descriptors from which our descriptor sets will draw from
        //
        // we could have multiple descriptor pools, or one huge one
        // we are going to have one per pipeline in order to keep it simple
        std::array <VkDescriptorPoolSize, 3u> const pool_sizes =
        {
            {
                {
                    // we have 1 x descriptor set that consists of 2 x SBO descriptors...
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 2u
                },
                {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1u
                },
                {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1u
                }
            }
        };
        if (!create_vulkan_descriptor_pool(_Device,
            1u, // how many descriptor sets will we make from the sets in the pool?
            pool_sizes.size(), pool_sizes.data(),
            _DescriptorPoolCompute))
        {
            DBG_ASSERT(false);
        }

        std::array <vulkan_descriptor_set_info, NUM_SETS_COMPUTE> const descriptor_set_infos =
        {
            {
                // set 0
                {
                  .desc_pool = &_DescriptorPoolCompute,  // the pool from which to allocate the individual descriptors from
                  .layout = &_DescriptorSetLayoutsCompute[0],    // layout of the descriptor set
                  .set_index = 0,   // index of the descriptor set
                  .out_set = &_DescSet0Compute   // pointer to where to instantiate the descriptor set to
                }
            }
        };
        if (!create_vulkan_descriptor_sets(_Device,
            descriptor_set_infos.size(), descriptor_set_infos.data()))
        {
            DBG_ASSERT(false);
        }
    }

    {
        if (!create_vulkan_buffer(_PhysicalDevice, _Device,
            _ParticleContainer->_MaxParticles * sizeof(float), 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            _BufferXPos))
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_buffer(_PhysicalDevice, _Device,
            _ParticleContainer->_MaxParticles * sizeof(float),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            _BufferYPos))
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_buffer(_PhysicalDevice, _Device,
            sizeof(compute_UBO_info_buffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            _BufferInfo))
        {
            DBG_ASSERT(false);
        }
    }

    {
        // desc_set_0_compute = _BufferXPos, _BufferYPos, buffer_info

        VkDescriptorBufferInfo const buffer_infos[NUM_RESOURCES_COMPUTE_SET_0] =
        {
          {
            .buffer = _BufferXPos.buffer,
            .offset = 0u,
            .range = VK_WHOLE_SIZE
          },
          {
            .buffer = _BufferYPos.buffer,
            .offset = 0u,
            .range = VK_WHOLE_SIZE
          },
          {
            .buffer = _BufferInfo.buffer,
            .offset = 0u,
            .range = VK_WHOLE_SIZE
          }
        };

        VkWriteDescriptorSet const write_descriptors[NUM_RESOURCES_COMPUTE_SET_0] =
        {
            // desc_set_0_compute
            {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              //.pNext = VK_NULL_HANDLE,
              .dstSet = _DescSet0Compute.desc_set,
              .dstBinding = BINDING_ID_SET_0_SBO_XPOS,
              .dstArrayElement = 0u,
              .descriptorCount = 1u,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .pImageInfo = VK_NULL_HANDLE,
              .pBufferInfo = &buffer_infos[0], // _BufferXPos
              .pTexelBufferView = VK_NULL_HANDLE
            },
            {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              //.pNext = VK_NULL_HANDLE,
              .dstSet = _DescSet0Compute.desc_set,
              .dstBinding = BINDING_ID_SET_0_SBO_YPOS,
              .dstArrayElement = 0u,
              .descriptorCount = 1u,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .pImageInfo = VK_NULL_HANDLE,
              .pBufferInfo = &buffer_infos[1],         // _BufferYPos
              .pTexelBufferView = VK_NULL_HANDLE
            },
            {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              //.pNext = VK_NULL_HANDLE,
              .dstSet = _DescSet0Compute.desc_set,
              .dstBinding = BINDING_ID_SET_0_UBO_INFO,
              .dstArrayElement = 0u,
              .descriptorCount = 1u,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pImageInfo = VK_NULL_HANDLE,
              .pBufferInfo = &buffer_infos[2], // buffer_info
              .pTexelBufferView = VK_NULL_HANDLE
            }
        };

        vkUpdateDescriptorSets(_Device, // device
            NUM_RESOURCES_COMPUTE_SET_0,  // descriptorWriteCount
            write_descriptors,            // pDescriptorWrites
            0u,                           // descriptorCopyCount
            VK_NULL_HANDLE);              // pDescriptorCopies
    }
    // create command buffers
    // at least one per pipeline
    {
        if (!create_vulkan_command_pool(VK_QUEUE_COMPUTE_BIT, _CommandPoolCompute))
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_command_buffers(1u, _CommandPoolCompute, &_CommandBufferCompute))
        {
            DBG_ASSERT(false);
        }
    }
    // create sync objects
    // at least one per submit (can be reused if in loop)
    {
        if (!create_vulkan_semaphores(1u, &_SemaphoreComputeComplete))
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_fences(1u, 0u, &_FenceCompute))
        {
            DBG_ASSERT(false);
        }
    }

    // SET INPUT/INFO BUFFERS
    {
        // Fill XPos Buffer memory with the particle X Positions
        if (!map_and_unmap_memory(_Device,
            _BufferXPos.memory, [&](void* mapped_memory)
        {
            f32* data = (f32*)mapped_memory;

            // Perform a memory copy to replicate the particle positions to the buffer
            memcpy(data, _ParticleContainer->_PosX,
                _ParticleContainer->_MaxParticles * sizeof(float));
        }))
        {
            DBG_ASSERT(false);
        }

        // Fill XPos Buffer memory with the particle Y Positions

        if (!map_and_unmap_memory(_Device,
            _BufferYPos.memory, [&](void* mapped_memory)
        {
            f32* data = (f32*)mapped_memory;

            // Perform a memory copy to replicate the particle positions to the buffer
            memcpy(data, _ParticleContainer->_PosY,
                _ParticleContainer->_MaxParticles * sizeof(float));
        }))
        {
            DBG_ASSERT(false);
        }

        // no need to set/initialise 'buffer_output' as its content will be completely overwritten by the compute shader

        // Fill '_BufferInfo.memory' with default compute info buffer
        if (!map_and_unmap_memory(_Device,
            _BufferInfo.memory, [&](void* mapped_memory)
        {
            compute_UBO_info_buffer* data = (compute_UBO_info_buffer*)mapped_memory;

            memcpy(data, &_DefaultComputeInfoBuffer, sizeof(compute_UBO_info_buffer));
        }))
        {
            DBG_ASSERT(false);
        }
    }
}

void VulkanParticleMachine::CreateGraphicsPipeline()
{
    // create graphics pipeline
    {
        // describe the descriptors in set 0
        std::array <VkDescriptorSetLayoutBinding, NUM_RESOURCES_GRAPHICS_SET_0> const descriptor_set_layout_binding_info_0 =
        {
          VkDescriptorSetLayoutBinding {
            .binding = BINDING_ID_SET_0_UBO_CAMERA,              // at binding point 0 we have
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // a uniform buffer
            .descriptorCount = 1u,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = VK_NULL_HANDLE
          },
          VkDescriptorSetLayoutBinding {
            .binding = BINDING_ID_SET_0_UBO_MODEL,               // at binding point 1 we have
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // another uniform buffer
            .descriptorCount = 1u,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = VK_NULL_HANDLE
          }
        };
        // describe the descriptors in set 1
        std::array <VkDescriptorSetLayoutBinding, NUM_RESOURCES_GRAPHICS_SET_1> const descriptor_set_layout_binding_info_1 =
        {
          VkDescriptorSetLayoutBinding {
            .binding = BINDING_ID_SET_1_COLOUR,               // at binding point 1 we have
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // another uniform buffer
            .descriptorCount = 1u,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = VK_NULL_HANDLE
          }
        };
        // group up all descriptor layout descriptions
        std::array <std::span <const VkDescriptorSetLayoutBinding>, NUM_SETS_GRAPHICS> const descriptor_set_layout_bindings =
        {
          descriptor_set_layout_binding_info_0, // set 0
          descriptor_set_layout_binding_info_1  // set 1
        };
        if (!create_vulkan_descriptor_set_layouts(_Device,
            NUM_SETS_GRAPHICS,
            descriptor_set_layout_bindings.data(),
            _DescriptorSetLayoutsGraphics.data()))
        {
            DBG_ASSERT(false);
        }

        if (!create_vulkan_pipeline_layout(_Device,
            _DescriptorSetLayoutsGraphics.size(), _DescriptorSetLayoutsGraphics.data(),
            0u, nullptr,
            _PipelineLayoutGraphics))
        {
            DBG_ASSERT(false);
        }

        VkShaderModule shader_module_graphics_vert = VK_NULL_HANDLE;
        if (!create_vulkan_shader(_Device,
            COMPILED_GRAPHICS_SHADER_PATH_VERT,
            shader_module_graphics_vert))
        {
            DBG_ASSERT(false);
        }

        VkShaderModule shader_module_graphics_frag = VK_NULL_HANDLE;
        if (!create_vulkan_shader(_Device,
            COMPILED_GRAPHICS_SHADER_PATH_FRAG,
            shader_module_graphics_frag))
        {
            DBG_ASSERT(false);
        }

        VkViewport const viewport =
        {
          .x = 0.f,
          .y = 0.f,
          .width = (f32)_SwapChainExtent.width,
          .height = (f32)_SwapChainExtent.height,
          .minDepth = 0.f,
          .maxDepth = 1.f
        };
        VkRect2D const scissor =
        {
          .offset = { 0, 0 },
          .extent = _SwapChainExtent
        };


        VkVertexInputBindingDescription vertex_input_binding_descriptions[] =
        {
            {
                .binding = 0u,
                .stride = sizeof(vec2),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            },
            {
                .binding = 1u,
                .stride = sizeof(f32),
                .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
            },
            {
                .binding = 2u,
                .stride = sizeof(f32),
                .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
            }
        };

        VkVertexInputAttributeDescription vertex_input_attribute_descriptions[] =
        {
            // position
            {
              .location = 0u,
              .binding = 0u,
              .format = VK_FORMAT_R32G32_SFLOAT,
              .offset = 0u
            },
            // instance x pos
            {
              .location = 2u,
              .binding = 1u,
              .format = VK_FORMAT_R32_SFLOAT,
              .offset = 0u
            },
            // instance y pos
            {
              .location = 3u,
              .binding = 2u,
              .format = VK_FORMAT_R32_SFLOAT,
              .offset = sizeof(f32)
            }
        };

        VkPipelineVertexInputStateCreateInfo const pvisci =
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          //.pNext = VK_NULL_HANDLE,
          //.flags = 0u,
          .vertexBindingDescriptionCount = 3u,
          .pVertexBindingDescriptions = vertex_input_binding_descriptions,
          .vertexAttributeDescriptionCount = 3u,
          .pVertexAttributeDescriptions = vertex_input_attribute_descriptions
        };

        VkPipelineInputAssemblyStateCreateInfo const piasci =
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          //.pNext = VK_NULL_HANDLE,
          //.flags = 0u,
          .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          .primitiveRestartEnable = VK_FALSE
        };

        VkPipelineRasterizationStateCreateInfo const prsci =
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          //.pNext = VK_NULL_HANDLE,
          //.flags = 0u,
          .depthClampEnable = VK_FALSE,
          .rasterizerDiscardEnable = VK_FALSE,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .cullMode = VK_CULL_MODE_BACK_BIT,
          .frontFace = VK_FRONT_FACE_CLOCKWISE,
          .depthBiasEnable = VK_FALSE,
          .depthBiasConstantFactor = 0.f,
          .depthBiasClamp = 0.f,
          .depthBiasSlopeFactor = 0.f,
          .lineWidth = 1.f
        };

        VkPipelineMultisampleStateCreateInfo const pmsci =
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          //.pNext = VK_NULL_HANDLE,
          //.flags = 0u,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
          .sampleShadingEnable = VK_FALSE,
          .minSampleShading = 0.f,
          .pSampleMask = nullptr,
          .alphaToCoverageEnable = VK_FALSE,
          .alphaToOneEnable = VK_FALSE
        };

        VkPipelineDepthStencilStateCreateInfo const pdssci =
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          //.pNext = VK_NULL_HANDLE,
          //.flags = 0u,
          .depthTestEnable = VK_FALSE,
          .depthWriteEnable = VK_FALSE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
          .depthBoundsTestEnable = VK_FALSE,
          .stencilTestEnable = VK_FALSE,
          .minDepthBounds = 0.f,
          .maxDepthBounds = 1.f
        };

        VkPipelineColorBlendAttachmentState const pipeline_colour_blend_attachment_states[] =
        {
          {
            .blendEnable = VK_FALSE,

            // if you want alpha blending, colour.rgb = (srcColor * SRC_ALPHA) ADD (dstColor * ONE_MINUS_SRC_ALPHA)
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,

            // colour.rgb = (srcColor * ONE) ADD (dstColor * ZERO)
            //.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            //.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,

            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
              | VK_COLOR_COMPONENT_G_BIT
              | VK_COLOR_COMPONENT_B_BIT
              | VK_COLOR_COMPONENT_A_BIT
          }
        };
        VkPipelineColorBlendStateCreateInfo const pcbsci =
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          //.pNext = VK_NULL_HANDLE,
          //.flags = 0u,
          .logicOpEnable = VK_FALSE,
          .logicOp = VK_LOGIC_OP_NO_OP,
          .attachmentCount = 1u,
          .pAttachments = pipeline_colour_blend_attachment_states,
          .blendConstants = { 0.f, 0.f, 0.f, 0.f }
        };

        if (!create_vulkan_pipeline_graphics(_Device,
            shader_module_graphics_vert, "main",
            shader_module_graphics_frag, "main",
            viewport, scissor,
            pvisci,
            piasci,
            prsci,
            pmsci,
            pdssci,
            pcbsci,
            _PipelineLayoutGraphics, _RenderPass, 0u,
            _PipelineGraphics))
        {
            DBG_ASSERT(false);
        }

        release_vulkan_shader(_Device,
            shader_module_graphics_frag); // don't need shader object now we have the pipeline
        release_vulkan_shader(_Device,
            shader_module_graphics_vert);
    }

    // create graphics descriptor sets
    {
        // to support n-buffering, each frame must have it's own instances of the descriptor sets, each with their own resources
        // i.e. resources (should) not be used simultaneously across frames
        // we are double buffering framebuffers, because Vulkan forces us to...
        // BUT, we are syncing between each frame to keep things simple, so one instance is enough

        // create pool of descriptors from which our descriptor sets will draw from
        //
        // we could have multiple descriptor pools, or one huge one
        // we are going to have one per pipeline in order to keep it simple
        std::array <VkDescriptorPoolSize, 1u> const pool_sizes =
        { 
        {
            {
                // we have 2 x descriptors set that consist of 2 x uniform buffers
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 3u
            }
        } 
        };
        if (!create_vulkan_descriptor_pool(_Device,
            NUM_SETS_GRAPHICS, // how many descriptor sets will we make from the sets in the pool?
            pool_sizes.size(), pool_sizes.data(),
            _DescriptorPoolGraphics))
        {
            DBG_ASSERT(false);
        }

        std::array <vulkan_descriptor_set_info, NUM_SETS_GRAPHICS> const descriptor_set_infos =
        { 
        {
            // set 0
            {
                .desc_pool = &_DescriptorPoolGraphics,         // the pool from which to allocate the individual descriptors from
                .layout = &_DescriptorSetLayoutsGraphics[0], // layout of the descriptor set
                .set_index = 0u,                                // index of the descriptor set
                .out_set = &_DescSet0Graphics           // pointer to where to instantiate the descriptor set to
            },
            // set 1
            {
                .desc_pool = &_DescriptorPoolGraphics,
                .layout = &_DescriptorSetLayoutsGraphics[1],
                .set_index = 1u,
                .out_set = &_DescSet1Graphics
            }
        } 
        };
        if (!create_vulkan_descriptor_sets(_Device,
            descriptor_set_infos.size(), descriptor_set_infos.data()))
        {
            DBG_ASSERT(false);
        }
    }
    // create graphics resources
    {

        if (!create_vulkan_buffer(_PhysicalDevice, _Device,
            sizeof(camera_buffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            _BufferGraphicsCamera))
        {
            DBG_ASSERT(false);
        }

        if (!create_vulkan_buffer(_PhysicalDevice, _Device,
            sizeof(model_buffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            _BufferGraphicsModel))
        {
            DBG_ASSERT(false);
        }

        if (!create_vulkan_buffer(_PhysicalDevice, _Device,
            sizeof(vec4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            _BufferGraphicsColour))
        {
            DBG_ASSERT(false);
        }

        // Load the particle texure and store it as a vulkan texure object

        VkFormat const image_format = VK_FORMAT_R8G8B8A8_UNORM;

        VkFormatProperties format_properties = {};
        // Get device properties for the requested texture format
        vkGetPhysicalDeviceFormatProperties(_PhysicalDevice, image_format, &format_properties);


        // as the data will never change (because we are not actually double buffering and the camera/sprites will never move)
        // we can set the data in these buffers upfront

        if (!map_and_unmap_memory(_Device,
            _BufferGraphicsCamera.memory,
            [&](void* mem)
            {
                camera_buffer* buf = (camera_buffer*)mem;

                // put camera origin in centre of screen
                f32 const z_near = 0.f, z_far = 1.f;

                buf->vp_matrix = glm::ortho(0.f, (f32)_SwapChainExtent.width, (f32)_SwapChainExtent.height, 0.f, z_near, z_far);
            }))
        {
            DBG_ASSERT(false);
        }

        if (!map_and_unmap_memory(_Device,
            _BufferGraphicsModel.memory,
            [](void* mem)
            {
                model_buffer* buf = (model_buffer*)mem;

                buf->model_matrix = fast_transform_2D(0.f, 0.f,
                    0.f,
                    0.f, 0.f,
                    1.f, 1.f);
            }))
        {
            DBG_ASSERT(false);
        }

        if (!map_and_unmap_memory(_Device,
            _BufferGraphicsColour.memory,
            [](void* mem)
            {
                *(vec4*)mem = vec4{1.0f,1.0f,1.0f,1.0f}; // Set the Particle Colour to white
            }))
        {
            DBG_ASSERT(false);
        }

    }
    // bind graphics + compute resources to graphics descriptor sets
    {
        
        // _DescSet0Graphics  = _BufferGraphicsCamera + _BufferGraphicsModel
        // _DescSet1Graphics  = _BufferGraphicsColour

        VkDescriptorBufferInfo const buffer_infos[] =
        {
          {
            .buffer = _BufferGraphicsCamera.buffer,
            .offset = 0u,
            .range = VK_WHOLE_SIZE
          },
          {
            .buffer = _BufferGraphicsModel.buffer,
            .offset = 0u,
            .range = VK_WHOLE_SIZE
          },
          {
            .buffer = _BufferGraphicsColour.buffer,
            .offset = 0u,
            .range = VK_WHOLE_SIZE
          },
        };

        VkWriteDescriptorSet const write_descriptors[] =
        {
            // DescSet0Graphics - binding 0
            {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              //.pNext = VK_NULL_HANDLE,
              .dstSet = _DescSet0Graphics.desc_set,
              .dstBinding = BINDING_ID_SET_0_UBO_CAMERA,
              .dstArrayElement = 0u,
              .descriptorCount = 1u,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pImageInfo = VK_NULL_HANDLE,
              .pBufferInfo = &buffer_infos[0], // BufferGraphicsCamera
              .pTexelBufferView = VK_NULL_HANDLE
            },
            // DescSet0Graphics - binding 1
            {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              //.pNext = VK_NULL_HANDLE,
              .dstSet = _DescSet0Graphics.desc_set,
              .dstBinding = BINDING_ID_SET_0_UBO_MODEL,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pImageInfo = VK_NULL_HANDLE,
              .pBufferInfo = &buffer_infos[1], // BufferGraphicsModel
              .pTexelBufferView = VK_NULL_HANDLE
            },

            // DescSet1Graphics - binding 0
            {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              //.pNext = VK_NULL_HANDLE,
              .dstSet = _DescSet1Graphics.desc_set,
              .dstBinding = BINDING_ID_SET_1_COLOUR,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // combined image sampler
              .pImageInfo = VK_NULL_HANDLE,
              .pBufferInfo = &buffer_infos[2], // _BufferGraphicsColour
              .pTexelBufferView = VK_NULL_HANDLE
            }
        };

        vkUpdateDescriptorSets(_Device, // device
            3u,                           // descriptorWriteCount
            write_descriptors,            // pDescriptorWrites
            0u,                           // descriptorCopyCount
            VK_NULL_HANDLE);              // pDescriptorCopies
    }
    // create model data
    {
        vec2 const vertices[] =
        {
          { -0.5f,  0.5f }, // 0, top left
          {  0.5f,  0.5f }, // 1, top right
          { -0.5f, -0.5f }, // 2, bottom left
          {  0.5f, -0.5f }, // 3, bottom right
        };
        u16 const indices[] = // clockwise order
        {
          0u, 1u, 2u, // face 0
          2u, 1u, 3u, // face 1
        };

        if (!create_vulkan_mesh(_PhysicalDevice, _Device,
            (void*)vertices, 4u, sizeof(vec2),
            (void*)indices, 6u, sizeof(u16),
            _MeshSprite))
        {
            DBG_ASSERT(false);
        }
    }
    // create graphics command buffers
    {
        // we can have several command buffers per pipeline
        // but we are going to keep it simple
        // 1 command buffer for graphics that we reuse every frame

        if (!create_vulkan_command_pool(VK_QUEUE_GRAPHICS_BIT, _CommandPoolGraphics))
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_command_buffers(1u, _CommandPoolGraphics, &_CommandBufferGraphics))
        {
            DBG_ASSERT(false);
        }
    }
    // create graphics sync objects
    // at least one per submit (can be reused if in loop)
    {
        if (!create_vulkan_semaphores(1u, &_SwapchainImageAvailableSemaphore))
        {
            DBG_ASSERT(false);
        }
        if (!create_vulkan_fences(1u, 0u, &_FenceSubmitGraphics))
        {
            DBG_ASSERT(false);
        }
    }
}

void VulkanParticleMachine::Update(float DeltaTime)
{
    UpdateImGui();

    // If the update is combined, start the performance timer
    if (!_SeparateUpdate)
    {
        _UpdateTimer.restart();
    }

    BindAndSubmitCompute(DeltaTime);

    BindAndSubmitGraphics();

    if (!_SeparateUpdate)
    {
        // If the compute and graphics are allowed to overlap we can retrieve particle data at the end
        // after the graphics, giving time for the compute to finish
        RetrieveParticleData();
        _UpdateTime = _UpdateTimer.total_elapsed();
        _UpdateTimeTotal += _UpdateTime;
        ++_UpdateCount;
    }
    else
    {
        // Increment the number of separated updates
        ++_SeparateUpdateCount;
    }
}

void VulkanParticleMachine::BindAndSubmitCompute(float DeltaTime)
{
    // If the update is separated, start a compute performance timer
    if (_SeparateUpdate)
    {
        _ComputeUpdateTimer.restart();
    }

    // RECORD COMMAND BUFFER
    {
        if (!begin_command_buffer(_CommandBufferCompute, 0u))
        {
            DBG_ASSERT(false);
        }
        {
            // any compute related command after this point is attached to this pipeline (on this command buffer)

            // call vkCmdBindPipeline
            vkCmdBindPipeline(_CommandBufferCompute, VK_PIPELINE_BIND_POINT_COMPUTE, _PipelineCompute);

            // bind descriptor set - _BufferXPos + _BufferYPos + _BufferInfo

            // bind our DSI (_DescSet0Compute.desc_set) to the pipeline
            vkCmdBindDescriptorSets(_CommandBufferCompute, VK_PIPELINE_BIND_POINT_COMPUTE, _PipelineLayoutCompute, _DescSet0Compute.set_index, 1,
                &_DescSet0Compute.desc_set, 0, NULL);

            // Push the delta time adjusted velocities
            compute_velocity_push_constants const data =
            {
              .x_deltavel = _ParticleContainer->_XVel * DeltaTime,
              .y_deltavel = _ParticleContainer->_YVel * DeltaTime,
            };

            vkCmdPushConstants(_CommandBufferCompute, _PipelineLayoutCompute, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                sizeof(compute_velocity_push_constants), &data);

            // trigger compute shader
            u32 const thread_group_dim = 256u; // this is set in the shader

            u32 group_count_x = (u32)glm::ceil((f32)_ParticleContainer->_MaxParticles / (f32)thread_group_dim);

            vkCmdDispatch(_CommandBufferCompute, group_count_x, 1, 1);

        }
        if (!end_command_buffer(_CommandBufferCompute))
        {
            DBG_ASSERT(false);
        }
    }

    // SUBMIT COMPUTE
    {

        vkResetFences(_Device, 1, &_FenceCompute);

        // submit compute commands
        VkSubmitInfo const submit_info =
        {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          //.pNext = VK_NULL_HANDLE,
          .waitSemaphoreCount = 0u,
          .pWaitSemaphores = VK_NULL_HANDLE,          // wait for these semaphores to be signalled before submitting command buffers
          .pWaitDstStageMask = VK_NULL_HANDLE,
          .commandBufferCount = 1u,
          .pCommandBuffers = &_CommandBufferCompute,
          .signalSemaphoreCount = 1u,
          .pSignalSemaphores = &_SemaphoreComputeComplete         // semaphores to trigger when command buffer has finished executing
        };
        if (!CHECK_VULKAN_RESULT(vkQueueSubmit(_QueueCompute, 1u, &submit_info,
            _FenceCompute))) // fence to signal when complete
        {
            DBG_ASSERT(false);
        }
    }

    if (_SeparateUpdate)
    {
        // If we are performing Compute Seperately from the graphics we need to wait for the 
        // compute to finish and retrieve the data before proceeding
        RetrieveParticleData();

        // Update performance timer data with the timer
        _ComputeUpdateTime = _ComputeUpdateTimer.total_elapsed();
        _ComputeUpdateTimeTotal += _ComputeUpdateTime;
    }
}

void VulkanParticleMachine::BindAndSubmitGraphics()
{
    // If the update is separated, start the graphics performance timer
    if (_SeparateUpdate)
    {
        _GraphicsUpdateTimer.restart();
    }
    // this semaphore will be triggered when a swapchain image is available to be rendered to
    if (!acquire_next_swapchain_image(_SwapchainImageAvailableSemaphore))
    {
        DBG_ASSERT(false);
    }

    // RECORD COMMAND BUFFER(S)
    {
        // we are reusing the graphics command buffer, so need to ensure it starts of empty
        if (!CHECK_VULKAN_RESULT(vkResetCommandBuffer(_CommandBufferGraphics, 0u)))
        {
            DBG_ASSERT(false);
        }

        if (!begin_command_buffer(_CommandBufferGraphics, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        {
            DBG_ASSERT(false);
        }


        begin_render_pass(_CommandBufferGraphics, { 0.f, 0.f, 0.f });

        // any graphics related command after this point is attached to this pipeline (on this command buffer)
        // bind graphics pipeline
        vkCmdBindPipeline(_CommandBufferGraphics, VK_PIPELINE_BIND_POINT_GRAPHICS, _PipelineGraphics);

        // render compute image
        {
            VkDeviceSize offsets[1] = { 0u }; // use me for 'pOffsets' and 'offset'


            // bind vertices
            vkCmdBindVertexBuffers(_CommandBufferGraphics, 0, 1, &_MeshSprite.buffer_vertex, offsets);

            // bind instancing x positions
            vkCmdBindVertexBuffers(_CommandBufferGraphics, 1, 1, &_BufferXPos.buffer, offsets);

            // bind instancing y positions
            vkCmdBindVertexBuffers(_CommandBufferGraphics, 2, 1, &_BufferYPos.buffer, offsets);

            // bind indices
            vkCmdBindIndexBuffer(_CommandBufferGraphics, _MeshSprite.buffer_index, *offsets, VK_INDEX_TYPE_UINT16);

            // bind graphics descriptor set 0 - buffer_graphics_camera + buffer_graphics_model_input
            vkCmdBindDescriptorSets(_CommandBufferGraphics, VK_PIPELINE_BIND_POINT_GRAPHICS, _PipelineLayoutGraphics, _DescSet0Graphics.set_index, 1,
                &_DescSet0Graphics.desc_set, 0, NULL);
            //  bind graphics descriptor set 1 - Colour Input
            vkCmdBindDescriptorSets(_CommandBufferGraphics, VK_PIPELINE_BIND_POINT_GRAPHICS, _PipelineLayoutGraphics, _DescSet1Graphics.set_index, 1,
                &_DescSet1Graphics.desc_set, 0, NULL);

            // call vkCmdDrawIndexed
            u32 InstanceCount = _ParticleContainer->_MaxParticles;
            vkCmdDrawIndexed(_CommandBufferGraphics, _MeshSprite.num_indices, InstanceCount, 0, 0, 0);


            // Render ImGui
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), _CommandBufferGraphics);
        }


        end_render_pass(_CommandBufferGraphics);


        if (!end_command_buffer(_CommandBufferGraphics))
        {
            DBG_ASSERT(false);
        }
    }

    // SUBMIT GRAPHICS
    {
        if (!CHECK_VULKAN_RESULT(vkResetFences(_Device, 1u, &_FenceSubmitGraphics)))
        {
            DBG_ASSERT(false);
        }


        // submit graphics commands
        VkPipelineStageFlags GraphicsWaitStageMask[2] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo const submit_info =
        {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          //.pNext = VK_NULL_HANDLE,
          .waitSemaphoreCount = 2,
          .pWaitSemaphores = _GraphicsWaitSemaphores, // wait for these semaphores to be signalled before submitting command buffers
          .pWaitDstStageMask = GraphicsWaitStageMask,
          .commandBufferCount = 1,
          .pCommandBuffers = &_CommandBufferGraphics,
          .signalSemaphoreCount = 0u,
          .pSignalSemaphores = VK_NULL_HANDLE                     // semaphores to trigger when command buffer has finished executing
        };

        if (!CHECK_VULKAN_RESULT(vkQueueSubmit(_QueueGraphics, 1u, &submit_info,
            _FenceSubmitGraphics))) // fence to signal when complete
        {
            DBG_ASSERT(false);
        }


        // wait for fence to be triggered via vkQueueSubmit
        // would not do this here if 'properly' double buffering...
        // research it
        if (!CHECK_VULKAN_RESULT(vkWaitForFences(_Device,
            1u,
            &_FenceSubmitGraphics,
            VK_TRUE,
            UINT64_MAX)))
        {
            DBG_ASSERT(false);
        }
    }

    if (!present())
    {
        DBG_ASSERT(false);
    }

    // If the update is separated, get the graphics performance data using the timer
    if (_SeparateUpdate)
    {
        _GraphicsUpdateTime = _GraphicsUpdateTimer.total_elapsed();
        _GraphicsUpdateTimeTotal += _GraphicsUpdateTime;
    }
}

void VulkanParticleMachine::RetrieveParticleData()
{
    // wait for fence to be triggered via vkQueueSubmit (slow!)
    vkWaitForFences(_Device, 1, &_FenceCompute, true, UINT64_MAX);

    // Copy the X positions back from the buffer
    if (!map_and_unmap_memory(_Device,
        _BufferXPos.memory, [&](void* mapped_memory)
    {
        f32* data = (f32*)mapped_memory;

        memcpy(_ParticleContainer->_PosX, data,
            _ParticleContainer->_MaxParticles * sizeof(float));
    }))
    {
        DBG_ASSERT(false);
    }

    // Copy the Y positions back from the buffer
    if (!map_and_unmap_memory(_Device,
        _BufferYPos.memory, [&](void* mapped_memory)
    {
        f32* data = (f32*)mapped_memory;

        memcpy(_ParticleContainer->_PosY, data,
            _ParticleContainer->_MaxParticles * sizeof(float));
    }))
    {
        DBG_ASSERT(false);
    }
}

void VulkanParticleMachine::Release()
{
    // Destroy ImGui Descriptor Pool
    vkDestroyDescriptorPool(_Device, _ImGuiPool, nullptr);
    //release_vulkan_command_buffers(1, _CommandPoolCompute, &_CommandBufferImGui);

    // Release pipelines in reverse order to how they were created
    ReleaseGraphicsPipeline();

    ReleaseComputePipeline();

    // Release context resources
    {
        release_vulkan_swapchain();

        release_vulkan_device();

        release_vulkan_surface();

        release_vulkan_instance();
        _QueueCompute = VK_NULL_HANDLE;
        _Device = VK_NULL_HANDLE;
        _PhysicalDevice = VK_NULL_HANDLE;
    }
}

void VulkanParticleMachine::ReleaseGraphicsPipeline()
{
    // GRAPHICS PIPELINE

    release_vulkan_fences(1u, &_FenceSubmitGraphics);
    release_vulkan_semaphores(1u, &_SwapchainImageAvailableSemaphore);

    release_vulkan_mesh(_Device, _MeshSprite);

    release_vulkan_command_buffers(1u, _CommandPoolGraphics, &_CommandBufferGraphics);
    release_vulkan_command_pool(_CommandPoolGraphics);

    release_vulkan_buffer(_Device, _BufferGraphicsModel);
    release_vulkan_buffer(_Device, _BufferGraphicsCamera);
    release_vulkan_buffer(_Device, _BufferGraphicsColour);

    release_vulkan_descriptor_sets(_Device,
        1u, &_DescSet1Graphics);
    release_vulkan_descriptor_sets(_Device,
        1u, &_DescSet0Graphics);

    release_vulkan_descriptor_pool(_Device, _DescriptorPoolGraphics);

    release_vulkan_pipeline(_Device, _PipelineGraphics);
    release_vulkan_pipeline_layout(_Device, _PipelineLayoutGraphics);
    release_vulkan_descriptor_set_layouts(_Device,
        NUM_SETS_GRAPHICS, _DescriptorSetLayoutsGraphics.data());
}

void VulkanParticleMachine::ReleaseComputePipeline()
{
    // COMPUTE PIPELINE
    release_vulkan_fences(1u, &_FenceCompute);
    release_vulkan_semaphores(1u, &_SemaphoreComputeComplete);

    release_vulkan_command_buffers(1u, _CommandPoolCompute, &_CommandBufferCompute);
    release_vulkan_command_pool(_CommandPoolCompute);

    release_vulkan_buffer(_Device, _BufferInfo);
    release_vulkan_buffer(_Device, _BufferXPos);
    release_vulkan_buffer(_Device, _BufferYPos);

    release_vulkan_descriptor_sets(_Device,
        1u, &_DescSet0Compute);
    release_vulkan_descriptor_pool(_Device, _DescriptorPoolCompute);

    release_vulkan_pipeline(_Device, _PipelineCompute);
    release_vulkan_pipeline_layout(_Device, _PipelineLayoutCompute);
    release_vulkan_descriptor_set_layouts(_Device,
        NUM_SETS_COMPUTE, _DescriptorSetLayoutsCompute.data());
}

void VulkanParticleMachine::SetImGuiCallback(void(*DrawCallback)())
{
    _DrawCallback = DrawCallback;
}

void VulkanParticleMachine::InitialiseImGui()
{
    //1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (!CHECK_VULKAN_RESULT(vkCreateDescriptorPool(_Device, &pool_info, nullptr, &_ImGuiPool)))
    {
        DBG_ASSERT(false);
    }

    // 2: initialize imgui library

    //this initializes the core structures of imgui
    ImGui::CreateContext();

    //this initializes imgui for GLFW
    ImGui_ImplGlfw_InitForVulkan(get_window_handle(), true);
    //this initializes imgui for Vulkan

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = get_vulkan_instance();
    init_info.PhysicalDevice = _PhysicalDevice;
    init_info.Device = _Device;
    init_info.Queue = _QueueGraphics;
    init_info.DescriptorPool = _ImGuiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, _RenderPass);

    //execute a gpu command to upload imgui font textures

    //create_vulkan_command_buffers(1, _CommandPoolCompute, &_CommandBufferImGui);
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    begin_single_time_commands(cmd);
    begin_command_buffer(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    end_command_buffer(cmd);

    end_single_time_commands();

    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void VulkanParticleMachine::UpdateImGui()
{
    //imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    // Call the draw callback function if one exists
    if (_DrawCallback)
    {
        _DrawCallback();
    }

    // Draw a window with performance data from within the Vulkan Particle Machine

    if (ImGui::Begin("Vulkan Particle Machine"))
    {
        // Checkbox for toggling if the updates should be combined for separated into graphics and compute
        ImGui::Checkbox("Seperate Update", &_SeparateUpdate);

        // Display different performance data for if the update is separated or combined
        if (!_SeparateUpdate && _UpdateCount > 0)
        {
            ImGui::Text("Update Time(us): %lli", _UpdateTime);
            ImGui::Text("Avg Update Time(us): %lli", _UpdateTimeTotal / _UpdateCount);
        }
        else if(_SeparateUpdate && _SeparateUpdateCount > 0)
        {
            ImGui::Text("Compute Time(us): %lli", _ComputeUpdateTime);
            ImGui::Text("Avg Compute Time(us): %lli", _ComputeUpdateTimeTotal / _SeparateUpdateCount);
            ImGui::Text("Graphics Time(us): %lli", _GraphicsUpdateTime);
            ImGui::Text("Avg Graphics Time(us): %lli", _GraphicsUpdateTimeTotal / _SeparateUpdateCount);

        }
        ImGui::End();
    }

    ImGui::Render();
}