#pragma once
#include <string>
#include "VertexInput.h"

class Image
{
public:
	void Create(VkDevice& device, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& graphicsQueue, const std::string& dir);
	void CreateView(VkDevice& device, SwapChain& swapChain);
	void CreateSampler(VkDevice& device, VkPhysicalDevice& physicalDevice);
	void CleanUp(VkDevice& device);
	static VkImageView CreateImageView(VkDevice& device, VkImage image, VkFormat format);

	VkImageView& GetView() { return m_textureImageView; };
	VkSampler& GetSampler() { return m_textureSampler; };

private:
	void CreateImage(VkDevice& device, VkPhysicalDevice& physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	
	VkImage m_textureImage;
	VkImageView m_textureImageView;
	VkSampler m_textureSampler;

	VkDeviceMemory m_textureImageMemory;

};

