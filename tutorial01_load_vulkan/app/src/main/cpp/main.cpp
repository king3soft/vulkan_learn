// Copyright 2022 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <android/log.h>
#include <cassert>
#include <vector>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "vulkan_wrapper.h"
#include "vkbootstrap/VkBootstrap.h"
#include "vk_layerhelper.hpp"
#include "vk_engine.h"

// Android log function wrappers
static const char* kTAG = "VKEngine";
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Vulkan call wrapper
#define CALL_VK(func)                                                                                                \
    if (VK_SUCCESS != (func)) {                                                                                      \
        __android_log_print(ANDROID_LOG_ERROR, "Tutorial ", "Vulkan error. File[%s], line[%d]", __FILE__, __LINE__); \
        assert(false);                                                                                               \
    }

#define VK_CHECK(func)                                                                                               \
    if (VK_SUCCESS != (func)) {                                                                                      \
        __android_log_print(ANDROID_LOG_ERROR, "Tutorial ", "Vulkan error. File[%s], line[%d]", __FILE__, __LINE__); \
        assert(false);                                                                                               \
    }

// Global variables
VulkanEngine vkEngine{};

// We will call this function the window is opened.
// This is where we will initialise everything
bool initialized_ = false;
bool initialize(android_app* app);

// Functions interacting with Android native activity
void android_main(struct android_app* state);
void handle_cmd(android_app* app, int32_t cmd);

// typical Android NativeActivity entry function
void android_main(struct android_app* app) {
    app->onAppCmd = handle_cmd;

    int events;
    android_poll_source* source;
    do {
        if (ALooper_pollAll(initialized_ ? 1 : 0, nullptr, &events, (void**)&source) >= 0) {
            if (source != NULL)
                source->process(app, source);
        }

        // render if vulkan is ready
        if (vkEngine._isInitialized) {
            vkEngine.draw();
        }
    } while (app->destroyRequested == 0);
}

bool initialize(android_app* app) {
    if (!InitVulkan()) {
        LOGE("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    vkEngine.init(app);    

    // Debug
    LayerAndExtensions layerHelper{};
    layerHelper.printLayers();
    layerHelper.printExtensions();

    initialized_ = true;
    return 0;
}

void terminate(void) {
    vkEngine.cleanup();
}

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            initialize(app);
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            terminate();
            break;
        default:
            LOGI("event not handled: %d", cmd);
    }
}