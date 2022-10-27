#pragma once
// #include "vk_types.h"
#include <iostream>
#include <queue>
#include <functional>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "vulkan_wrapper.h"
#include "vma/vk_mem_alloc.h"
#include "vk_mesh.h"

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) { deletors.push_back(function); }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)();  // call functors
        }

        deletors.clear();
    }
};

class VulkanEngine {
   public:
    android_app* _app;

   public:
    VkInstance _instance;                       // Vulkan library handle
    VkDebugUtilsMessengerEXT _debug_messenger;  // Vulkan debug output handle
    VkSurfaceKHR _surface;                      // Vulkan window surface

    VkDevice _device;             // Vulkan device for commands
    VkPhysicalDevice _chosenGPU;  // GPU chosen as the default device

   public:                      // swap chain
    VkSwapchainKHR _swapchain;  // from other articles
    VkExtent2D _windowExtent;
    VkFormat _swapchainImageFormat;                 // image format expected by the windowing system
    std::vector<VkImage> _swapchainImages;          // array of images from the swapchain
    std::vector<VkImageView> _swapchainImageViews;  // array of image-views from the swapchain

   public:
    VkQueue _graphicsQueue;         // queue we will submit to
    uint32_t _graphicsQueueFamily;  // family of that queue

    VkCommandPool _commandPool;          // the command pool for our commands
    VkCommandBuffer _mainCommandBuffer;  // the buffer we will record into
   public:
    VkRenderPass _renderPass;

   public:  // sync
    VkSemaphore _presentSemaphore, _renderSemaphore;
    VkFence _renderFence;

   public:
    std::vector<VkFramebuffer> _framebuffers;

   private:
    VkPipelineLayout _trianglePipelineLayout;
    VkPipelineLayout _meshPipelineLayout;
    VkPipeline _trianglePipeline;
    VkPipeline _meshPipeline;

    Mesh _triangleMesh;

   private:
    VmaAllocator _allocator;  // vma lib allocator
   private:
    DeletionQueue _mainDeletionQueue;

   public:
    bool _isInitialized{false};
    int _frameNumber{0};

    VulkanEngine() : _instance{}, _surface{} {};

    // initializes everything in the engine
    void init(android_app* app);

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();

    // run main loop
    void run();

   private:
    void init_vulkan(android_app* app);
    void init_vma();
    void init_swapchain(android_app* app);
    void init_commands();
    void init_default_renderpass();
    void init_framebuffers();
    void init_sync_structures();
    // shader module

    // loads a shader module from a spir-v file. Returns false if it errors
    bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);
    void init_pipelines();

    //
    void load_meshes();
    void upload_mesh(Mesh& mesh);
};

class PipelineBuilder {
   public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkViewport _viewport;
    VkRect2D _scissor;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;

    VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};