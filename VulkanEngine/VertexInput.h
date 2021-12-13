#pragma once
#include "Vertex.h"
class SwapChain;

class VertexInput
{
public:
	void CreateBuffers(VkDevice& device, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& graphicsQueue, SwapChain& swapChain);
	void CreateVertexBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& graphicsQueue);
	void CreateIndexBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& graphicsQueue);
	void CreateUniformBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, SwapChain& swapChain);

	VkBuffer& GetVertexBuffer() { return m_vertexBuffer; };
	VkBuffer& GetIndicesBuffer() { return m_indexBuffer; };
	
	std::vector<VkDeviceMemory>& GetUniformBuffersMemory() { return uniformBuffersMemory; };
	std::vector<VkBuffer>& GetUniformBuffers() { return uniformBuffers; };
	size_t GetIndicesSize() const { return m_indices.size(); };

	void CleanUp(VkDevice& device, const std::vector<VkImage>& swapChainImages);

private:
	uint32_t FindMemoryType(VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	void CreateBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CopyBuffer(VkDevice& device, VkCommandPool& commandPool, VkQueue& graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;

	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexBufferMemory;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;

	const std::vector<Vertex> m_vertices =
	{
		{{-0.5f, -0.5f},  glm::vec3(1.0f,0.0f,0.0f)},
		{{0.5f, -0.5f}, glm::vec3(0.0f,1.0f,0.0f)},
		{{0.5f, 0.5f},  glm::vec3(0.0f,0.0f,1.0f)},
		{{-0.5f, 0.5f}, glm::vec3(0.0f)}
	};

	const std::vector<uint16_t> m_indices = 
	{
		0, 1, 2, 2, 3, 0
	};
};

