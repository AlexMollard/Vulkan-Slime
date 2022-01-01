//
// Created by alexm on 23/12/2021.
//

#pragma once

#include <vulkan/vulkan.h>
#include "VulkanInitializers.h"
#include "VkBootstrap.h"

class VulkanEngine {
public:
    // --- omitted ---
    VkInstance mInstance; // Vulkan library handle
    VkDebugUtilsMessengerEXT mDebugMessenger; // Vulkan debug output handle
    VkPhysicalDevice mChosenGPU; // GPU chosen as the default device
    VkDevice mDevice; // Vulkan device for commands
    VkSurfaceKHR mSurface; // Vulkan window surface

    VkSwapchainKHR mSwapchain; // from other articles

    // image format expected by the windowing system
    VkFormat mSwapchainImageFormat;

    //array of images from the swapchain
    std::vector<VkImage> mSwapchainImages;

    //array of image-views from the swapchain
    std::vector<VkImageView> mSwapchainImageViews;

    VkQueue mGraphicsQueue; //queue we will submit to
    uint32_t mGraphicsQueueFamily; //family of that queue

    VkCommandPool mCommandPool; //the command pool for our commands
    VkCommandBuffer mMainCommandBuffer; //the buffer we will record into

    VkRenderPass mRenderPass;

    std::vector<VkFramebuffer> mFramebuffers;

    VkSemaphore mPresentSemaphore, mRenderSemaphore;
    VkFence mRenderFence;

    //initializes everything in the engine
    void init();

    //shuts down the engine
    void cleanup();

    //draw loop
    void draw();

    //run main loop
    void run();

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();

    void init_default_renderpass();

    void init_framebuffers();

    void init_sync_structures();

    struct SDL_Window *mWindow{nullptr};
    bool mIsInitialized{false};
    int mFrameNumber{0};

    VkExtent2D mWindowExtent{800, 600};
};
