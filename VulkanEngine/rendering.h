#pragma once

#include "vulkan/vulkan.h"
#include <vector>

class deviceAndQueue;

class swapChain;

class window;

class vertexInput;

class image;

class rendering {
public:
    void
    draw(deviceAndQueue &deviceAndQueue, swapChain &swapChain, window &window, vertexInput &vertexBuffer, image &image);

    void createSyncObjects(const VkDevice &device, swapChain &swapChain);

    void cleanUp(VkDevice &device);

private:
    static void updateUniform(uint32_t currentImage, VkDevice &device, swapChain &swapChain, vertexInput &vertexInput);

    const int maxFramesInFlight = 2;
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence> mInFlightFences;
    std::vector<VkFence> mImagesInFlight;
    size_t mCurrentFrame = 0;
};

