//
// Created by alexm on 21/12/2021.
//

#include "mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc

#include "tiny_obj_loader.h"

#include <iostream>
#include <unordered_map>
#include "vertexInput.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/hash.hpp>

template<>
struct std::hash<vertex> {
    size_t operator()(vertex const &vertex) const {
        return ((std::hash<glm::vec3>()(vertex.pos) ^ (std::hash<glm::vec3>()(vertex.colour) << 1)) >> 1) ^
               (std::hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};

mesh::mesh() = default;

void mesh::cleanUp(VkDevice &device) {
    vkDestroyBuffer(device, mIndexBuffer, nullptr);
    vkFreeMemory(device, mIndexBufferMemory, nullptr);

    vkDestroyBuffer(device, mVertexBuffer, nullptr);
    vkFreeMemory(device, mVertexBufferMemory, nullptr);
}

void mesh::init(deviceAndQueue &devices, VkCommandPool &commandPool, const std::string &inputfile) {
    mMeshData.startVertIndex = 0;
    mMeshData.startIndex = 0;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputfile.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<vertex, uint32_t> uniqueVertices{};

    for (const auto &shape: shapes) {
        for (const auto &index: shape.mesh.indices) {
            vertex bufferVertex{};

            bufferVertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
            };

            bufferVertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            bufferVertex.colour = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(bufferVertex) == 0) {
                uniqueVertices[bufferVertex] = static_cast<uint32_t>(mVertexs.size());
                mVertexs.push_back(bufferVertex);
            }

            mIndices.push_back(uniqueVertices[bufferVertex]);
        }
    }

    vertexInput::createIndexBuffer(devices, commandPool, mIndices, mIndexBuffer, mIndexBufferMemory);
    vertexInput::createVertexBuffer(devices, commandPool, mVertexs, mVertexBuffer, mVertexBufferMemory);
}
