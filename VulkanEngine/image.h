#pragma once

#include <string>
#include "vertexInput.h"

class image {
public:
    void create(VkDevice &device, VkPhysicalDevice &physicalDevice, VkCommandPool &commandPool, VkQueue &graphicsQueue,
                const std::string &dir);

    void createView(VkDevice &device, swapChain &swapChain);

    void createSampler(VkDevice &device, VkPhysicalDevice &physicalDevice);

    void cleanUp(VkDevice &device);

    static VkImageView createImageView(VkDevice &device, VkImage image, VkFormat format);

    VkImageView &getView() { return mTextureImageView; };

    VkSampler &getSampler() { return mTextureSampler; };

private:
    static void
    createImage(VkDevice &device, VkPhysicalDevice &physicalDevice, uint32_t width, uint32_t height, VkFormat format,
                VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image,
                VkDeviceMemory &imageMemory);

    VkImage mTextureImage;
    VkImageView mTextureImageView;
    VkSampler mTextureSampler;

    VkDeviceMemory mTextureImageMemory;
};

