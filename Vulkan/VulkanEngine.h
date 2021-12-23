//
// Created by alexm on 23/12/2021.
//

#pragma once

#include <vulkan/vulkan.h>

class VulkanEngine {
public:
    //initializes everything in the engine
    void init();

    //shuts down the engine
    void cleanup();

    //draw loop
    void draw();

    //run main loop
    void run();

private:
    struct SDL_Window *mWindow{nullptr};
    bool mIsInitialized{false};
    int mFrameNumber{0};

    VkExtent2D mWindowExtent{800, 600};
};