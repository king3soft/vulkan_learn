#include "vk_mesh.h"
#include <stb/tiny_obj_loader.h>

VertexInputDescription Vertex::get_vertex_description() {
    VertexInputDescription description;

    // we will have just 1 vertex buffer binding, with a per-vertex rate
    VkVertexInputBindingDescription mainBinding = {};
    mainBinding.binding                         = 0;
    mainBinding.stride                          = sizeof(Vertex);
    mainBinding.inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

    description.bindings.push_back(mainBinding);

    // Position will be stored at Location 0
    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding                           = 0;
    positionAttribute.location                          = 0;
    positionAttribute.format                            = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset                            = offsetof(Vertex, position);

    // Normal will be stored at Location 1
    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding                           = 0;
    normalAttribute.location                          = 1;
    normalAttribute.format                            = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset                            = offsetof(Vertex, normal);

    // Color will be stored at Location 2
    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding                           = 0;
    colorAttribute.location                          = 2;
    colorAttribute.format                            = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset                            = offsetof(Vertex, color);

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);
    return description;
}

bool Mesh::load_from_obj(const char* filename) {
    // attrib will contain the vertex arrays of the file
    tinyobj::attrib_t attrib;

    // shapes contains the info for each separate object in the file
    std::vector<tinyobj::shape_t> shapes;

    // materials contains the information about the material of each shape, but we won't use it.
    std::vector<tinyobj::material_t> materials;

    // error and warning output from the load function
    std::string warn;
    std::string err;

    // load the OBJ file
    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);

    // make sure to output the warnings to the console, in case there are issues with the file
    if (!warn.empty()) {
        LOGE("WARN: %s", warn.c_str());
    }

    // if we have any error, print it to the console, and break the mesh loading.
    // This happens if the file can't be found or is malformed
    if (!err.empty()) {
		LOGE("ERROR: %s", err.c_str());
        return false;
    }
};