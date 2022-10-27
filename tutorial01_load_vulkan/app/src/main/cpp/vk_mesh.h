#pragma once

//#include <vk_types.h>
#include <vector>
#include <glm/vec3.hpp>
#include "vma/vk_mem_alloc.h"
struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;    
    static VertexInputDescription get_vertex_description();
};

struct AllocatedBuffer {
    VkBuffer _buffer;
    VmaAllocation _allocation;
};

struct Mesh {
    std::vector<Vertex> _vertices;

    AllocatedBuffer _vertexBuffer;
};


