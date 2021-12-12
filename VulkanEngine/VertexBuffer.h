#pragma once
#include <vulkan/vulkan.h>

#include "glm/glm.hpp"

#include <vector>
#include <array>

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 colour;

	static VkVertexInputBindingDescription GetBindingDecription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	};

	static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, colour);

		return attributeDescriptions;
	}
};

class VertexBuffer
{
public:
	void CreateVertexBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& graphicsQueue);
	void CreateIndexBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& graphicsQueue);

	VkBuffer& GetVertexBuffer() { return m_vertexBuffer; };
	VkBuffer& GetIndicesBuffer() { return m_indexBuffer; };
	
	size_t GetIndicesSize() const { return m_indices.size(); };

	void CleanUp(VkDevice& device);

private:
	uint32_t FindMemoryType(VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	void CreateBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CopyBuffer(VkDevice& device, VkCommandPool& commandPool, VkQueue& graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;

	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexBufferMemory;

	glm::vec3 color = glm::vec3( 78.0f, 201.0f, 176.0f ) / glm::vec3(255.0f);
	
	const std::vector<Vertex> m_vertices =
	{
		{{-1.0f, -0.5f}, color},
		{{1.0f, -0.5f}, color},
		{{1.0f, 0.5f}, color},
		{{-1.0f, 0.5f}, color}
	};

	const std::vector<uint16_t> m_indices = 
	{
		0, 1, 2, 2, 3, 0
	};
};

