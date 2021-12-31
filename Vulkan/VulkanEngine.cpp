//
// Created by alexm on 23/12/2021.
//

#include "VulkanEngine.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#include <iostream>

//we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                     \
	do                                                                  \
	{                                                                   \
		VkResult err = x;                                               \
		if (err)                                                        \
		{                                                               \
			std::cout <<"Detected Vulkan error: " << err << std::endl;  \
			abort();                                                    \
		}                                                               \
	} while (0)

void VulkanEngine::init() {
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN;

    //create blank SDL window for our application
    mWindow = SDL_CreateWindow(
            "Vulkan Slime", //window title
            SDL_WINDOWPOS_UNDEFINED, //window position x
            SDL_WINDOWPOS_UNDEFINED, //window position y
            mWindowExtent.width,  //window width in pixels
            mWindowExtent.height, //window height in pixels
            windowFlags
    );

    //load the core Vulkan structures
    init_vulkan();

    //create the swapchain
    init_swapchain();

    //Create Command Pools and command buffers
    init_commands();

    //everything went fine
    mIsInitialized = true;
}

void VulkanEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

    //make the Vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Slime Vulkan")
            .request_validation_layers(true)
            .require_api_version(1, 1, 0)
            .use_default_debug_messenger()
            .build();

    vkb::Instance vkb_inst = inst_ret.value();

    //store the instance
    mInstance = vkb_inst.instance;
    //store the debug messenger
    mDebugMessenger = vkb_inst.debug_messenger;

    // get the surface of the window we opened with SDL
    SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

    //use vkbootstrap to select a GPU.
    //We want a GPU that can write to the SDL surface and supports Vulkan 1.1
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
            .set_minimum_version(1, 1)
            .set_surface(mSurface)
            .select()
            .value();

    //create the final Vulkan device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a Vulkan application
    mDevice = vkbDevice.device;
    mChosenGPU = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain() {
    vkb::SwapchainBuilder swapchainBuilder{mChosenGPU,mDevice,mSurface };

    vkb::Swapchain vkbSwapchain = swapchainBuilder
            .use_default_format_selection()
                    //use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(mWindowExtent.width, mWindowExtent.height)
            .build()
            .value();

    //store swapchain and its related images
    mSwapchain = vkbSwapchain.swapchain;
    mSwapchainImages = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    mSwapchainImageFormat = vkbSwapchain.image_format;
}

void VulkanEngine::init_commands() {
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(mGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mCommandPool));

    //allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(mCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mMainCommandBuffer));
}

void VulkanEngine::cleanup() {
    if (mIsInitialized) {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

        //destroy swapchain resources
        for (int i = 0; i < mSwapchainImageViews.size(); i++) {

            vkDestroyImageView(mDevice, mSwapchainImageViews[i], nullptr);
        }

        vkDestroyDevice(mDevice, nullptr);
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
        vkDestroyInstance(mInstance, nullptr);
        SDL_DestroyWindow(mWindow);
    }
}

void VulkanEngine::draw() {
    //nothing yet
}

void VulkanEngine::run() {
    SDL_Event e;
    bool shouldClose = false;

    //main loop
    while (!shouldClose) {
        //Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            //close the window when user clicks the X button or alt-f4s
            if (e.type == SDL_QUIT) shouldClose = true;
        }

        draw();
    }
}
