#pragma once

#include "debugging.h"
#include "vulkan/vulkan.h"

#include <optional>
#include <vector>
#include <map>
#include <iostream>
#include <set>

class swapChain;

struct queueFamilyIndices {
    std::optional<uint32_t> mGraphicsFamily;
    std::optional<uint32_t> mPresentFamily;

    [[nodiscard]] bool isComplete() const {
        return (mGraphicsFamily.has_value() && mPresentFamily.has_value());
    };
};

class deviceAndQueue {
public:
    void pickPhysicalDevice(VkInstance &instance, VkSurfaceKHR &surface, swapChain &swapChain);

    void createLogicalDevice(const bool &enableValidationLayers, const std::vector<const char *> &validationLayers);

    queueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

    VkDevice &getDevice() { return mDevice; };

    VkPhysicalDevice &getPhysicalDevice() { return mPhysicalDevice; };

    VkQueue &getGraphicsQueue() { return mGraphicsQueue; };

    VkQueue &getPresentQueue() { return mPresentQueue; };
private:
    static uint32_t getDeviceSuitability(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

    bool isDeviceSuitable(const VkPhysicalDevice &device);

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice{};
    VkQueue mGraphicsQueue{};
    VkQueue mPresentQueue{};
    VkSurfaceKHR *mSurface{};
    swapChain *mSwapChain{};
};

