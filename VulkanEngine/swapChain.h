#pragma once

#include "window.h"
#include "vertexInput.h"

#include <optional>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <iostream>
#include <set>

class deviceAndQueue;

class image;

struct swapChainSupportDetails {
    VkSurfaceCapabilitiesKHR mCapabilities;
    std::vector<VkSurfaceFormatKHR> mFormats;
    std::vector<VkPresentModeKHR> mPresentModes;
};

class swapChain {
public:
    void init(VkDevice &device, deviceAndQueue &deviceAndQueue, window &window);

    void createSwapChain(deviceAndQueue &deviceAndQueue, window &window);

    void recreateSwapChain(deviceAndQueue &deviceAndQueue, window &window, vertexInput &vertexBuffer, image &image);

    void createImageViews(VkDevice &device);

    void createRenderPass(const VkDevice &device);

    void createGraphicsPipeline(const VkDevice &device);

    void createFrameBuffer(const VkDevice &device);

    void createCommandBuffers(const VkDevice &device, vertexInput &vertexBuffer);

    void createCommandPool(deviceAndQueue &deviceAndQueue);

    void createDescriptorPool(VkDevice &device);

    void createDescriptorSets(VkDevice &device, vertexInput &vertexInput, image &image);

    VkDescriptorSetLayout &getDescriptorSetLayout() { return mDescriptorSetLayout; };

    void cleanUp(const VkDevice &device, vertexInput &vertexInput);

    VkSwapchainKHR &getSwapChain() { return mSwapChain; };

    std::vector<VkImage> &getSwapChainImages() { return mSwapChainImages; };

    VkCommandPool &getCommandPool() { return mCommandPool; };

    std::vector<VkCommandBuffer> &getCommandBuffers() { return mCommandBuffers; };

    const std::vector<const char *> &getDeviceExtensions() const { return mDeviceExtensions; };

    VkExtent2D &getSwapChainExtent() { return mSwapChainExtent; };

    static swapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR &surface);

private:
    void createDescriptorSetLayout(const VkDevice &device);

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

    static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow &window);

    VkSwapchainKHR mSwapChain;
    std::vector<VkImage> mSwapChainImages;
    VkFormat mSwapChainImageFormat;
    VkExtent2D mSwapChainExtent;
    std::vector<VkImageView> mSwapChainImageViews;

    VkPipelineLayout mPipelineLayout;
    VkRenderPass mRenderPass;
    VkPipeline mGraphicsPipeline;
    std::vector<VkFramebuffer> mSwapChainFramebuffers;
    VkCommandPool mCommandPool;
    std::vector<VkCommandBuffer> mCommandBuffers;

    const std::vector<const char *> mDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;
};

