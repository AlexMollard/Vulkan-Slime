#pragma once

#include <vulkan/vulkan.h>

#include "glm/glm.hpp"

#include <vector>
#include <array>

struct vertex {
    glm::vec3 pos;
    glm::vec3 colour;
    glm::vec2 texCoord;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();

    bool operator==(const vertex &other) const {
        return pos == other.pos && colour == other.colour && texCoord == other.texCoord;
    }
};

struct uniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};