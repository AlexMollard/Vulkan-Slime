#pragma once
#include <vulkan/vulkan.h>

#include "glm/glm.hpp"

#include <vector>
#include <array>

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 colour;

	static VkVertexInputBindingDescription GetBindingDecription();
	static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};