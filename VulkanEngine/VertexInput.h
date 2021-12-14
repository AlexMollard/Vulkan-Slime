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

	static void CreateBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	static uint32_t FindMemoryType(VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
	static void CopyBuffer(VkDevice& device, VkCommandPool& commandPool, VkQueue& graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	static void TransitionImageLayout(VkDevice& device, VkCommandPool& commandPool, VkQueue& graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	static void CopyBufferToImage(VkDevice& device, VkCommandPool& commandPool, VkQueue& graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

private:
	static VkCommandBuffer BeginSingleTimeCommands(VkDevice& device, VkCommandPool& commandPool);
	static void EndSingleTimeCommands(VkDevice& device, VkCommandBuffer commandBuffer, VkCommandPool& commandPool, VkQueue& graphicsQueue);

	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;

	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexBufferMemory;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;

	const std::vector<Vertex> m_vertices =
	{
		{glm::vec3(1.0f, 1.0f, -1.0f),		glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)},    // Top Right Of The Quad (Top)
		{glm::vec3(-1.0f, 1.0f, -1.0f),		glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)},    // Top Left Of The Quad (Top)
		{glm::vec3(-1.0f, 1.0f, 1.0f),		glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)},    // Bottom Left Of The Quad (Top)
		{glm::vec3(1.0f, 1.0f, 1.0f),		glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)},    // Bottom Right Of The Quad (Top)

		{glm::vec3(1.0f, -1.0f, 1.0f),		glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},    // Top Right Of The Quad (Bottom)
		{glm::vec3(-1.0f, -1.0f, 1.0f),		glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},    // Top Left Of The Quad (Bottom)
		{glm::vec3(-1.0f, -1.0f, -1.0f),	glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},    // Bottom Left Of The Quad (Bottom)
		{glm::vec3(1.0f, -1.0f, -1.0f),		glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},    // Bottom Right Of The Quad (Bottom

		{glm::vec3(1.0f, 1.0f, 1.0f),		glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)},    // Top Right Of The Quad (Front)
		{glm::vec3(-1.0f, 1.0f, 1.0f),		glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},    // Top Left Of The Quad (Front)
		{glm::vec3(-1.0f, -1.0f, 1.0f),		glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},    // Bottom Left Of The Quad (Front)
		{glm::vec3(1.0f, -1.0f, 1.0f),		glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},    // Bottom Right Of The Quad (Front)

		{glm::vec3(1.0f, -1.0f, -1.0f),		glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},    // Top Right Of The Quad (Back)
		{glm::vec3(-1.0f, -1.0f, -1.0f),	glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},    // Top Left Of The Quad (Back)
		{glm::vec3(-1.0f, 1.0f, -1.0f),		glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},    // Bottom Left Of The Quad (Back)
		{glm::vec3(1.0f, 1.0f, -1.0f),		glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},    // Bottom Right Of The Quad (Back)

		{glm::vec3(-1.0f, 1.0f, 1.0f),		glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)},    // Top Right Of The Quad (Left)
		{glm::vec3(-1.0f, 1.0f, -1.0f),		glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},    // Top Left Of The Quad (Left)
		{glm::vec3(-1.0f, -1.0f, -1.0f),	glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},    // Bottom Left Of The Quad (Left)
		{glm::vec3(-1.0f, -1.0f, 1.0f),		glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},    // Bottom Right Of The Quad (Left)

		{glm::vec3(1.0f, 1.0f, -1.0f),		glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)},    // Top Right Of The Quad (Right)
		{glm::vec3(1.0f, 1.0f, 1.0f),		glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(0.0f, 0.0f)},    // Top Left Of The Quad (Right)
		{glm::vec3(1.0f, -1.0f, 1.0f),		glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(0.0f, 1.0f)},    // Bottom Left Of The Quad (Right)
		{glm::vec3(1.0f, -1.0f, -1.0f),		glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(1.0f, 1.0f)}    // Bottom Right Of The Quad (Right)
	};

	const std::vector<uint16_t> m_indices = 
	{
		0, 1, 3,	// Top
		1, 2, 3,	// Top

		5, 7, 4,	// Bottom
		7, 5, 6,	// Bottom
		
		8, 9, 10,	// Front
		10, 11, 8,	//  Front
		
		12, 13, 14,	// Back
		14, 15, 12,	//  Back
		
		16, 17, 18,	// Left
		18, 19, 16,	//  Left
		
		20, 21, 22,	// Right
		22, 23, 20	//  Right
	};

};

