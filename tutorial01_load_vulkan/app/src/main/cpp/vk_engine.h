#pragma once
// #include "vk_types.h"
#include <iostream>
#include <queue>
#include <functional>
#include <map>
#include <unordered_map>
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

// note that we store the VkPipeline and layout by value, not pointer.
// They are 64 bit handles to internal driver structures anyway so storing pointers to them isn't very useful
struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 transformMatrix;
};

class VulkanEngine {
   public:
    android_app* _app;

    std::vector<RenderObject> _renderables;

    std::unordered_map<std::string, Material> _materials;
    std::unordered_map<std::string, Mesh> _meshes;

    // create material and add it to the map
    Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name) {
        Material mat;
        mat.pipeline       = pipeline;
        mat.pipelineLayout = layout;
        _materials[name]   = mat;
        return &_materials[name];
    }

    // returns nullptr if it can't be found
    Material* get_material(const std::string& name) {
        auto it = _materials.find(name);
        if (it == _materials.end()) {
            assert(false);
            return nullptr;
        }
        return &it->second;
    }

    // returns nullptr if it can't be found
    Mesh* get_mesh(const std::string& name) {
        auto it = _meshes.find(name);
        if (it == _meshes.end()) {
            assert(false);
            return nullptr;
        }
        return &it->second;
    }

    // our draw function
    void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count) {
        // make a model view matrix for rendering the object
        // camera view
        glm::vec3 camPos = {0.f, -6.f, -10.f};
        glm::mat4 view   = glm::translate(glm::mat4(1.f), camPos);
        // camera projection
        glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
        projection[1][1] *= -1;
        Mesh* lastMesh         = nullptr;
        Material* lastMaterial = nullptr;
        for (int i = 0; i < count; i++) {
            RenderObject& object = first[i];

            // only bind the pipeline if it doesn't match with the already bound one
            if (object.material != lastMaterial) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
                lastMaterial = object.material;
            }

            glm::mat4 model = object.transformMatrix;
            // final render matrix, that we are calculating on the cpu
            glm::mat4 mesh_matrix = projection * view * model;

            MeshPushConstants constants;
            constants.render_matrix = mesh_matrix;

            // upload the mesh to the GPU via push constants
            vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

            // only bind the mesh if it's a different one from last bind
            if (object.mesh != lastMesh) {
                // bind the mesh vertex buffer with offset 0
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
                lastMesh = object.mesh;
            }
            // we can now draw
            vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, 0);
        }
    }

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

   private:
    VkImageView _depthImageView;
    AllocatedImage _depthImage;

    // the format for the depth image
    VkFormat _depthFormat;

    Mesh _triangleMesh;
    Mesh _monkeyMesh;

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
    //
    void init_scene() {
        RenderObject monkey;
        monkey.mesh            = get_mesh("monkey");
        monkey.material        = get_material("defaultmesh");
        monkey.transformMatrix = glm::mat4{1.0f};

        _renderables.push_back(monkey);

        for (int x = -20; x <= 20; x++) {
            for (int y = -20; y <= 20; y++) {
                RenderObject tri;
                tri.mesh              = get_mesh("triangle");
                tri.material          = get_material("defaultmesh");
                glm::mat4 translation = glm::translate(glm::mat4{1.0}, glm::vec3(x, 0, y));
                glm::mat4 scale       = glm::scale(glm::mat4{1.0}, glm::vec3(0.2, 0.2, 0.2));
                tri.transformMatrix   = translation * scale;
                _renderables.push_back(tri);
            }
        }
    }
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
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineLayout _pipelineLayout;

    VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};