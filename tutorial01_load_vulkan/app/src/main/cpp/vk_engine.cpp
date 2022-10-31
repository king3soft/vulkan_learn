// #include "vk_init.h"
#include <android/log.h>
#include <vector>
#include "vk_engine.h"
#include "vkbootstrap/VkBootstrap.h"
#include "vk_init.h"
#include "log.h"
// #define VMA_IMPLEMENTATION
// #include "vk_mem_alloc.h"

// we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;

static const char* VkResultString(VkResult err) {
    switch (err) {
#define STR(r) \
    case r:    \
        return #r
        STR(VK_SUCCESS);
        STR(VK_NOT_READY);
        STR(VK_TIMEOUT);
        STR(VK_EVENT_SET);
        STR(VK_EVENT_RESET);
        STR(VK_INCOMPLETE);
        STR(VK_ERROR_OUT_OF_HOST_MEMORY);
        STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        STR(VK_ERROR_INITIALIZATION_FAILED);
        STR(VK_ERROR_DEVICE_LOST);
        STR(VK_ERROR_MEMORY_MAP_FAILED);
        STR(VK_ERROR_LAYER_NOT_PRESENT);
        STR(VK_ERROR_EXTENSION_NOT_PRESENT);
        STR(VK_ERROR_FEATURE_NOT_PRESENT);
        STR(VK_ERROR_INCOMPATIBLE_DRIVER);
        STR(VK_ERROR_TOO_MANY_OBJECTS);
        STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
        STR(VK_ERROR_FRAGMENTED_POOL);
        STR(VK_ERROR_OUT_OF_POOL_MEMORY);
        STR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        STR(VK_ERROR_SURFACE_LOST_KHR);
        STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(VK_SUBOPTIMAL_KHR);
        STR(VK_ERROR_OUT_OF_DATE_KHR);
        STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(VK_ERROR_VALIDATION_FAILED_EXT);
        STR(VK_ERROR_INVALID_SHADER_NV);
        STR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        STR(VK_ERROR_FRAGMENTATION_EXT);
        STR(VK_ERROR_NOT_PERMITTED_EXT);
#undef STR
        default:
            return "UNKNOWN_RESULT";
    }
}

void VulkanEngine::init(android_app* app) {
    this->_app = app;
    this->init_vulkan(app);
    this->init_vma();
    // load meshes
    this->load_meshes();

    // create the swapchain
    this->init_swapchain(app);
    this->init_commands();
    this->init_default_renderpass();
    this->init_framebuffers();
    this->init_sync_structures();
    this->init_pipelines();
    this->_isInitialized = true;
}

void VulkanEngine::cleanup() {
    if (_isInitialized) {
        vkDeviceWaitIdle(_device);

        vkFreeCommandBuffers(_device, _commandPool, 1, &_mainCommandBuffer);

        vkDestroyCommandPool(_device, _commandPool, nullptr);

        vkDestroySwapchainKHR(_device, _swapchain, nullptr);

        // destroy the main renderpass
        vkDestroyRenderPass(_device, _renderPass, nullptr);

        // destroy swapchain resources
        for (int i = 0; i < _framebuffers.size(); i++) {
            vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
            vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
        }

        //
        this->_mainDeletionQueue.flush();

        vkDestroyDevice(_device, NULL);
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyInstance(_instance, NULL);

        LOGI("VKEngine Cleanup");
    }
    this->_isInitialized = false;
}

void VulkanEngine::init_vma() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = _chosenGPU;
    allocatorInfo.device                 = _device;
    allocatorInfo.instance               = _instance;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_allocator));
}

void VulkanEngine::init_vulkan(android_app* app) {
    vkb::InstanceBuilder builder;

    // make the Vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(true)
                        .require_api_version(1, 1, 0)
                        //.use_default_debug_messenger()
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // store the instance
    _instance = vkb_inst.instance;
    // store the debug messenger
    //_debug_messenger = vkb_inst.debug_messenger;

    // if we create a surface, we need the surface extension
    VkAndroidSurfaceCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .pNext = nullptr, .flags = 0, .window = app->window};
    VK_CHECK(vkCreateAndroidSurfaceKHR(this->_instance, &createInfo, nullptr, &this->_surface));

    // use vkbootstrap to select a GPU.
    // We want a GPU that can write to the SDL surface and supports Vulkan 1.1
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 1).set_surface(_surface).select().value();

    // create the final Vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkb_Device = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a Vulkan application
    _device    = vkb_Device.device;
    _chosenGPU = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    _graphicsQueue       = vkb_Device.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkb_Device.get_queue_index(vkb::QueueType::graphics).value();
    LOGI("init vulkan end");
}

void VulkanEngine::init_swapchain(android_app* app) {
    _windowExtent = {(uint32_t)ANativeWindow_getWidth(app->window), (uint32_t)ANativeWindow_getHeight(app->window)};
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                      .use_default_format_selection()
                                      // use vsync present mode
                                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_composite_alpha_flags(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
                                      .set_desired_extent(_windowExtent.width, _windowExtent.height)
                                      .build()
                                      .value();

    // store swapchain and its related images
    _swapchain           = vkbSwapchain.swapchain;
    _swapchainImages     = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    _swapchainImageFormat = vkbSwapchain.image_format;

    // depth image size will match the window
    VkExtent3D depthImageExtent = {_windowExtent.width, _windowExtent.height, 1};

    // hardcoding the depth format to 32 bit float
    _depthFormat = VK_FORMAT_D32_SFLOAT;

    // the depth image will be an image with the format we selected and Depth Attachment usage flag
    VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

    // for the depth image, we want to allocate it from GPU local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags           = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);

    // build an image-view for the depth image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));
    // add to deletion queues
    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_device, _depthImageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
    });
}

void VulkanEngine::init_commands() {
    // create a command pool for commands submitted to the graphics queue.
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext                   = nullptr;

    // the command pool will be one that can submit graphics commands
    commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily;
    // we also want the pool to allow for resetting of individual command buffers
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext                       = nullptr;

    // commands will be made from our _commandPool
    cmdAllocInfo.commandPool = _commandPool;
    // we will allocate 1 command buffer
    cmdAllocInfo.commandBufferCount = 1;
    // command level is Primary
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));
}

void VulkanEngine::init_default_renderpass() {
    // the renderpass will use this color attachment.
    VkAttachmentDescription color_attachment = {};
    // the attachment will have the format needed by the swapchain
    color_attachment.format = _swapchainImageFormat;
    // 1 sample, we won't be doing MSAA
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // we Clear when this attachment is loaded
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // we keep the attachment stored when the renderpass ends
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // we don't care about stencil
    color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // we don't know or care about the starting layout of the attachment
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // after the renderpass ends, the image has to be on a layout ready for display
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment = {};
    // Depth attachment
    depth_attachment.flags          = 0;
    depth_attachment.format         = _depthFormat;
    depth_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment            = 1;
    depth_attachment_ref.layout                = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {};
    // attachment number will index into the pAttachments array in the parent renderpass itself
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_attachment_ref;
    // hook the depth attachment into the subpass
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    // array of 2 attachments, one for the color, and other for depth
    VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    // connect the color attachment to the info
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments    = &attachments[0];
    // connect the subpass to the info
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses   = &subpass;
    //
    VkSubpassDependency dependency = {};
    dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass          = 0;
    dependency.srcStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask       = 0;
    dependency.dstStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    //
    VkSubpassDependency depth_dependency = {};
    depth_dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
    depth_dependency.dstSubpass          = 0;
    depth_dependency.srcStageMask        = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.srcAccessMask       = 0;
    depth_dependency.dstStageMask        = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.dstAccessMask       = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    //
    VkSubpassDependency dependencies[2] = {dependency, depth_dependency};
    render_pass_info.dependencyCount    = 2;
    render_pass_info.pDependencies      = &dependencies[0];

    VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));
}

void VulkanEngine::init_framebuffers() {
    // create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext                   = nullptr;

    fb_info.renderPass      = _renderPass;
    fb_info.attachmentCount = 1;
    fb_info.width           = _windowExtent.width;
    fb_info.height          = _windowExtent.height;
    fb_info.layers          = 1;

    // grab how many images we have in the swapchain
    const uint32_t swapchain_imagecount = _swapchainImages.size();
    _framebuffers                       = std::vector<VkFramebuffer>(swapchain_imagecount);

    // create framebuffers for each of the swapchain image views
    for (int i = 0; i < swapchain_imagecount; i++) {
        VkImageView attachments[2];
        attachments[0]          = _swapchainImageViews[i];
        attachments[1]          = _depthImageView;
        fb_info.pAttachments    = &attachments[0];
        fb_info.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));
    }
}

void VulkanEngine::init_sync_structures() {
    // create synchronization structures
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext             = nullptr;

    // we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence));
    this->_mainDeletionQueue.push_function([=]() { vkDestroyFence(_device, _renderFence, nullptr); });

    // for the semaphores we don't need any flags
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext                 = nullptr;
    semaphoreCreateInfo.flags                 = 0;

    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));
    this->_mainDeletionQueue.push_function([=]() { vkDestroySemaphore(_device, _presentSemaphore, nullptr); });
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));
    this->_mainDeletionQueue.push_function([=]() { vkDestroySemaphore(_device, _renderSemaphore, nullptr); });
}

void VulkanEngine::draw() {
    // wait until the GPU has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &_renderFence));

    // request image from the swapchain, one second timeout
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _presentSemaphore, nullptr, &swapchainImageIndex));

    // now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

    // naming it cmd for shorter writing
    VkCommandBuffer cmd = _mainCommandBuffer;

    // begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext                    = nullptr;

    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // make a clear-color from frame number. This will flash with a 120*pi frame period.
    VkClearValue clearValue;
    float flash      = abs(sin(_frameNumber / 120.f));
    clearValue.color = {{0.0f, 0.0f, flash, 1.0f}};

    //clear depth at 1
	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;

    // start the main renderpass.
    // We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.pNext                 = nullptr;
    rpInfo.renderPass            = _renderPass;
    rpInfo.renderArea.offset.x   = 0;
    rpInfo.renderArea.offset.y   = 0;
    rpInfo.renderArea.extent     = _windowExtent;
    rpInfo.framebuffer           = _framebuffers[swapchainImageIndex];

    // connect clear values
    rpInfo.clearValueCount = 2;
    VkClearValue clearValues[] = { clearValue, depthClear };
    rpInfo.pClearValues    = &clearValues[0];
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    // bind the mesh vertex buffer with offset 0
    // VkDeviceSize offset = 0;
    // vkCmdBindVertexBuffers(cmd, 0, 1, &_triangleMesh._vertexBuffer._buffer, &offset);

    // bind the mesh vertex buffer with offset 0
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &_monkeyMesh._vertexBuffer._buffer, &offset);

    // make a model view matrix for rendering the object
    // camera position
    glm::vec3 camPos = {0.f, 0.f, -2.f};

    glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);

    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
    projection[1][1] *= -1;
    // model rotation
    glm::mat4 model = glm::rotate(glm::mat4{1.0f}, glm::radians(_frameNumber * 0.4f), glm::vec3(0, 1, 0));

    // calculate final mesh matrix
    glm::mat4 mesh_matrix = projection * view * model;

    MeshPushConstants constants;
    constants.render_matrix = mesh_matrix;

    // upload the matrix to the GPU via push constants
    vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

    // we can now draw the mesh
    // vkCmdDraw(cmd, _triangleMesh._vertices.size(), 1, 0, 0);

    // we can now draw the mesh
    vkCmdDraw(cmd, _monkeyMesh._vertices.size(), 1, 0, 0);

    // finalize the render pass
    vkCmdEndRenderPass(cmd);
    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the _renderSemaphore, to signal that rendering has finished
    VkSubmitInfo submit = {};
    submit.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext        = nullptr;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask       = &waitStage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores    = &_presentSemaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &_renderSemaphore;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    // submit command buffer to the queue and execute it.
    //_renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext            = nullptr;

    presentInfo.pSwapchains    = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores    = &_renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;
    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    // increase the number of frames drawn
    _frameNumber++;
}

bool VulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule) {
    auto AssetManager = this->_app->activity->assetManager;
    AAsset* file      = AAssetManager_open(AssetManager, filePath, AASSET_MODE_BUFFER);
    size_t fileLength = AAsset_getLength(file);

    char* fileContent = new char[fileLength];
    AAsset_read(file, fileContent, fileLength);

    const uint32_t* content = (const uint32_t*)fileContent;
    VkShaderModuleCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr, .flags = 0, .codeSize = fileLength, .pCode = content};

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }

    *outShaderModule = shaderModule;
    delete[] fileContent;

    return true;
}

void VulkanEngine::init_pipelines() {
    // shader module loading
    VkShaderModule vertexShader, fragmentShader;
    this->load_shader_module("shaders/tri.vert.spv", &vertexShader);
    this->load_shader_module("shaders/mesh.frag.spv", &fragmentShader);

    // build the pipeline layout that controls the inputs/outputs of the shader
    // we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

    // build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;
    pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
    pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

    // vertex input controls how to read vertices from vertex buffers. We aren't using it yet
    pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();  // default

    // input assembly is the configuration for drawing triangle lists, strips, or individual points.
    // we are just going to draw triangle list
    pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    // build viewport and scissor from the swapchain extents
    pipelineBuilder._viewport.x        = 0.0f;
    pipelineBuilder._viewport.y        = 0.0f;
    pipelineBuilder._viewport.width    = (float)_windowExtent.width;
    pipelineBuilder._viewport.height   = (float)_windowExtent.height;
    pipelineBuilder._viewport.minDepth = 0.0f;
    pipelineBuilder._viewport.maxDepth = 1.0f;

    pipelineBuilder._scissor.offset = {0, 0};
    pipelineBuilder._scissor.extent = _windowExtent;

    // configure the rasterizer to draw filled triangles
    pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    // we don't use multisampling, so just run the default one
    pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

    // a single blend attachment with no blending and writing to RGBA
    pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;

    // finally build the pipeline
    _trianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

    // build the mesh pipeline
    VertexInputDescription vertexDescription = Vertex::get_vertex_description();

    // connect the pipeline builder vertex input info to the one we get from Vertex
    pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions    = vertexDescription.attributes.data();
    pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

    pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions    = vertexDescription.bindings.data();
    pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

    // clear the shader stages for the builder
    pipelineBuilder._shaderStages.clear();

    // compile mesh vertex shader
    VkShaderModule meshVertShader;
    if (!this->load_shader_module("shaders/tri_mesh.vert.spv", &meshVertShader)) {
        LOGE("Error when building the triangle vertex shader module");
    } else {
        LOGI("tri_mesh.vert.spv vertex shader successfully loaded");
    }
    // add the other shaders
    pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
    // make sure that triangleFragShader is holding the compiled colored_triangle.frag
    pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

    // we start from just the default empty pipeline layout info
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VkPushConstantRange push_constant;
    push_constant.offset                             = 0;
    push_constant.size                               = sizeof(MeshPushConstants);
    push_constant.stageFlags                         = VK_SHADER_STAGE_VERTEX_BIT;
    mesh_pipeline_layout_info.pPushConstantRanges    = &push_constant;
    mesh_pipeline_layout_info.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &_meshPipelineLayout));

    pipelineBuilder._pipelineLayout = _meshPipelineLayout;

    // build the mesh triangle pipeline
    _meshPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

    // deleting all of the vulkan shaders
    vkDestroyShaderModule(_device, meshVertShader, nullptr);
    vkDestroyShaderModule(_device, vertexShader, nullptr);
    vkDestroyShaderModule(_device, fragmentShader, nullptr);

    // adding the pipelines to the deletion queue
    _mainDeletionQueue.push_function([=]() {
        vkDestroyPipeline(_device, _trianglePipeline, nullptr);
        vkDestroyPipeline(_device, _meshPipeline, nullptr);

        vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
        vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
    });
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass) {
    // make viewport state from our stored viewport and scissor.
    // at the moment we won't support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext                             = nullptr;

    viewportState.viewportCount = 1;
    viewportState.pViewports    = &_viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &_scissor;

    // setup dummy color blending. We aren't using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext                               = nullptr;

    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.logicOp         = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &_colorBlendAttachment;

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext                        = nullptr;

    pipelineInfo.stageCount          = _shaderStages.size();
    pipelineInfo.pStages             = _shaderStages.data();
    pipelineInfo.pVertexInputState   = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState   = &_multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = _pipelineLayout;
    pipelineInfo.renderPass          = pass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

    //other states
	pipelineInfo.pDepthStencilState = &_depthStencil;

    // it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        LOGE("failed to create pipeline");
        return VK_NULL_HANDLE;  // failed to create graphics pipeline
    } else {
        return newPipeline;
    }
}

void VulkanEngine::load_meshes() {
    // make the array 3 vertices long
    _triangleMesh._vertices.resize(3);

    // vertex positions
    _triangleMesh._vertices[0].position = {1.f, 1.f, 0.0f};
    _triangleMesh._vertices[1].position = {-1.f, 1.f, 0.0f};
    _triangleMesh._vertices[2].position = {0.f, -1.f, 0.0f};

    // vertex colors, all green
    _triangleMesh._vertices[0].color = {0.f, 1.f, 0.0f};  // pure green
    _triangleMesh._vertices[1].color = {0.f, 1.f, 0.0f};  // pure green
    _triangleMesh._vertices[2].color = {0.f, 1.f, 0.0f};  // pure green

    // load the monkey
    _monkeyMesh.load_from_obj(this->_app->activity->assetManager, "monkey_smooth.obj");

    // we don't care about the vertex normals
    upload_mesh(_triangleMesh);
    upload_mesh(_monkeyMesh);
}

void VulkanEngine::upload_mesh(Mesh& mesh) {
    // allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    // this is the total size, in bytes, of the buffer we are allocating
    bufferInfo.size = mesh._vertices.size() * sizeof(Vertex);
    // this buffer is going to be used as a Vertex Buffer
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    // let the VMA library know that this data should be writeable by CPU, but also readable by GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage                   = VMA_MEMORY_USAGE_CPU_TO_GPU;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &mesh._vertexBuffer._buffer, &mesh._vertexBuffer._allocation, nullptr));

    // add the destruction of triangle mesh buffer to the deletion queue
    _mainDeletionQueue.push_function([=]() { vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer, mesh._vertexBuffer._allocation); });

    // copy vertex data
    void* data;
    vmaMapMemory(_allocator, mesh._vertexBuffer._allocation, &data);
    memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(_allocator, mesh._vertexBuffer._allocation);
}

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"