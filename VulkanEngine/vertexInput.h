#pragma once

#include "vertex.h"

class swapChain;

class vertexInput {
public:
    void createBuffers(VkDevice &device, VkPhysicalDevice &physicalDevice, VkCommandPool &commandPool,
                       VkQueue &graphicsQueue, swapChain &swapChain);

    void createVertexBuffer(VkDevice &device, VkPhysicalDevice &physicalDevice, VkCommandPool &commandPool,
                            VkQueue &graphicsQueue);

    void createIndexBuffer(VkDevice &device, VkPhysicalDevice &physicalDevice, VkCommandPool &commandPool,
                           VkQueue &graphicsQueue);

    void createUniformBuffer(VkDevice &device, VkPhysicalDevice &physicalDevice, swapChain &swapChain);

    VkBuffer &getVertexBuffer() { return mVertexBuffer; };

    VkBuffer &getIndicesBuffer() { return mIndexBuffer; };

    std::vector<VkDeviceMemory> &getUniformBuffersMemory() { return uniformBuffersMemory; };

    std::vector<VkBuffer> &getUniformBuffers() { return uniformBuffers; };

    [[nodiscard]] size_t getIndicesSize() const { return mIndices.size(); };

    void cleanUp(VkDevice &device, const std::vector<VkImage> &swapChainImages);

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

    VkBuffer mVertexBuffer;
    VkDeviceMemory mVertexBufferMemory;

    VkBuffer mIndexBuffer;
    VkDeviceMemory mIndexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    const std::vector<vertex> mVertices =
            {
                    {glm::vec3(1.0f, 1.0f, -1.0f),   glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f,
                                                                                            0.0f)},    // Top Right Of The Quad (Top)
                    {glm::vec3(-1.0f, 1.0f, -1.0f),  glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f,
                                                                                            0.0f)},    // Top Left Of The Quad (Top)
                    {glm::vec3(-1.0f, 1.0f, 1.0f),   glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f,
                                                                                            1.0f)},    // Bottom Left Of The Quad (Top)
                    {glm::vec3(1.0f, 1.0f, 1.0f),    glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f,
                                                                                            1.0f)},    // Bottom Right Of The Quad (Top)

                    {glm::vec3(1.0f, -1.0f, 1.0f),   glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f,
                                                                                            0.0f)},    // Top Right Of The Quad (Bottom)
                    {glm::vec3(-1.0f, -1.0f, 1.0f),  glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f,
                                                                                            0.0f)},    // Top Left Of The Quad (Bottom)
                    {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f,
                                                                                            1.0f)},    // Bottom Left Of The Quad (Bottom)
                    {glm::vec3(1.0f, -1.0f, -1.0f),  glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f,
                                                                                            1.0f)},    // Bottom Right Of The Quad (Bottom

                    {glm::vec3(1.0f, 1.0f, 1.0f),    glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f,
                                                                                            0.0f)},    // Top Right Of The Quad (Front)
                    {glm::vec3(-1.0f, 1.0f, 1.0f),   glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f,
                                                                                            0.0f)},    // Top Left Of The Quad (Front)
                    {glm::vec3(-1.0f, -1.0f, 1.0f),  glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f,
                                                                                            1.0f)},    // Bottom Left Of The Quad (Front)
                    {glm::vec3(1.0f, -1.0f, 1.0f),   glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f,
                                                                                            1.0f)},    // Bottom Right Of The Quad (Front)

                    {glm::vec3(1.0f, -1.0f, -1.0f),  glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f,
                                                                                            1.0f)},    // Top Right Of The Quad (Back)
                    {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f,
                                                                                            1.0f)},    // Top Left Of The Quad (Back)
                    {glm::vec3(-1.0f, 1.0f, -1.0f),  glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f,
                                                                                            0.0f)},    // Bottom Left Of The Quad (Back)
                    {glm::vec3(1.0f, 1.0f, -1.0f),   glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f,
                                                                                            0.0f)},    // Bottom Right Of The Quad (Back)

                    {glm::vec3(-1.0f, 1.0f, 1.0f),   glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(1.0f,
                                                                                            0.0f)},    // Top Right Of The Quad (Left)
                    {glm::vec3(-1.0f, 1.0f, -1.0f),  glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(0.0f,
                                                                                            0.0f)},    // Top Left Of The Quad (Left)
                    {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(0.0f,
                                                                                            1.0f)},    // Bottom Left Of The Quad (Left)
                    {glm::vec3(-1.0f, -1.0f, 1.0f),  glm::vec3(1.0f, 0.0f, 1.0f), glm::vec2(1.0f,
                                                                                            1.0f)},    // Bottom Right Of The Quad (Left)

                    {glm::vec3(1.0f, 1.0f, -1.0f),   glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(1.0f,
                                                                                            0.0f)},    // Top Right Of The Quad (Right)
                    {glm::vec3(1.0f, 1.0f, 1.0f),    glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(0.0f,
                                                                                            0.0f)},    // Top Left Of The Quad (Right)
                    {glm::vec3(1.0f, -1.0f, 1.0f),   glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(0.0f,
                                                                                            1.0f)},    // Bottom Left Of The Quad (Right)
                    {glm::vec3(1.0f, -1.0f, -1.0f),  glm::vec3(0.0f, 1.0f, 1.0f), glm::vec2(1.0f,
                                                                                            1.0f)}    // Bottom Right Of The Quad (Right)
            };

    const std::vector<uint16_t> mIndices =
            {
                    0, 1, 3,    // Top
                    1, 2, 3,    // Top

                    5, 7, 4,    // Bottom
                    7, 5, 6,    // Bottom

                    8, 9, 10,    // Front
                    10, 11, 8,    //  Front

                    12, 13, 14,    // Back
                    14, 15, 12,    //  Back

                    16, 17, 18,    // Left
                    18, 19, 16,    //  Left

                    20, 21, 22,    // Right
                    22, 23, 20    //  Right
            };

};

