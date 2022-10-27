#[[
Copyright 2022 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
]]
cmake_minimum_required(VERSION 3.4.1)

project(vktuts)

# Integrate GameActivity, refer to
# https://d.android.com/games/agdk/integrate-game-activity
# for the detailed instructions.
find_package(game-activity REQUIRED CONFIG)

set(SRC_DIR ${CMAKE_SOURCE_DIR})
get_filename_component(REPO_ROOT_DIR
    ${CMAKE_SOURCE_DIR}/../../../../.. ABSOLUTE)
set(COMMON_DIR ${REPO_ROOT_DIR}/common)

add_library(glm INTERFACE)

add_library(${CMAKE_PROJECT_NAME} SHARED
    main.cpp
    vk_engine.cpp
    vk_mesh.cpp
    vk_layerhelper.cpp
    vkbootstrap/VkBootstrap.cpp
    ${COMMON_DIR}/vulkan_wrapper/vulkan_wrapper.cpp
    ${COMMON_DIR}/src/GameActivitySources.cpp)

set(LAYER_SRC_DIR ${ANDROID_NDK}/sources/third_party/vulkan/src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -Werror       \
                     -Wno-unused-variable -DUSE_DEBUG_EXTENTIONS \
                     -DVK_USE_PLATFORM_ANDROID_KHR")

target_include_directories(glm INTERFACE glm)
include_directories(${COMMON_DIR}/vulkan_wrapper ${LAYER_SRC_DIR}/include ${CMAKE_SOURCE_DIR}/glm)

target_link_libraries(${CMAKE_PROJECT_NAME}
    game-activity::game-activity
    log
    android)
