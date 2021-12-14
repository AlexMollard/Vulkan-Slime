#pragma once
#include <vulkan/vulkan.h>

#include "glm/glm.hpp"

#include <vector>
#include <array>

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 colour;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription GetBindingDecription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};