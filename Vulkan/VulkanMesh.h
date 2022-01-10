//
// Created by alexmollard on 6/1/22.
//

#pragma once

#include <VulkanTypes.h>
#include <vector>
#include <glm/glm.hpp>

struct VertexInputDescription {

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {

    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 color;

    static VertexInputDescription get_vertex_description();
};

struct Mesh {
    std::vector<Vertex> mVertices = {};

    vktype::AllocatedBuffer mVertexBuffer;

    bool load_from_obj(const char *filename);
};
