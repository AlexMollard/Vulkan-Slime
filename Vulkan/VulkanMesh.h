//
// Created by alexmollard on 6/1/22.
//

#pragma once

#include <VulkanTypes.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

constexpr bool logMeshUpload = false;


struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};



struct Vertex {

    glm::vec3 position;
    //glm::vec3 normal;
    glm::vec<2, uint8_t> oct_normal;//color;
    glm::vec<3, uint8_t> color;
    glm::vec2 uv;
    static VertexInputDescription get_vertex_description();

    void pack_normal(glm::vec3 n);
    void pack_color(glm::vec3 c);
};
struct RenderBounds {
    glm::vec3 origin;
    float radius;
    glm::vec3 extents;
    bool valid;
};
struct Mesh {
    std::vector<Vertex> mVertices;
    std::vector<uint32_t> mIndices;

    AllocatedBuffer<Vertex> mVertexBuffer;
    AllocatedBuffer<uint32_t> mIndexBuffer;

    RenderBounds bounds;

    bool load_from_meshasset(const char* filename);
};