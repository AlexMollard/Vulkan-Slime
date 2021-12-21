#pragma once

#include "vertex.h"
#include "deviceAndQueue.h"

class swapChain;

class vertexInput {
public:
    static void createVertexBuffer(deviceAndQueue &devices, VkCommandPool &commandPool, std::vector<vertex> &vertices,
                                   VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory);

    static void createIndexBuffer(deviceAndQueue &devices, VkCommandPool &commandPool, std::vector<uint32_t> &indices,
                                  VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory);

    void createUniformBuffer(deviceAndQueue &devices, swapChain &swapChain);

    std::vector<VkDeviceMemory> &getUniformBuffersMemory() { return mUniformBuffersMemory; };

    std::vector<VkBuffer> &getUniformBuffers() { return mUniformBuffers; };

    static void
    createBuffer(VkDevice &device, VkPhysicalDevice &physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                 VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);

    static uint32_t
    findMemoryType(VkPhysicalDevice &physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    static void copyBuffer(VkDevice &device, VkCommandPool &commandPool, VkQueue &graphicsQueue, VkBuffer srcBuffer,
                           VkBuffer dstBuffer, VkDeviceSize size);

    static void
    transitionImageLayout(VkDevice &device, VkCommandPool &commandPool, VkQueue &graphicsQueue, VkImage image,
                          VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    static void copyBufferToImage(VkDevice &device, VkCommandPool &commandPool, VkQueue &graphicsQueue, VkBuffer buffer,
                                  VkImage image, uint32_t width, uint32_t height);

private:
    static VkCommandBuffer beginSingleTimeCommands(VkDevice &device, VkCommandPool &commandPool);

    static void endSingleTimeCommands(VkDevice &device, VkCommandBuffer commandBuffer, VkCommandPool &commandPool,
                                      VkQueue &graphicsQueue);

    std::vector<VkBuffer> mUniformBuffers;
    std::vector<VkDeviceMemory> mUniformBuffersMemory;
};

