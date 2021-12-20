#pragma once

#include <vulkan/vulkan.h>

#include "glm/glm.hpp"

#include <vector>
#include <array>

struct vertex {
    glm::vec3 pos;
    glm::vec3 colour;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDecription();

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

struct uniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};