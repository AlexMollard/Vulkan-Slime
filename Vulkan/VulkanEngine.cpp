//
// Created by alexm on 23/12/2021.
//

#include "VulkanEngine.h"
#include <SDL.h>
#include <SDL_vulkan.h>

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

    //everything went fine
    mIsInitialized = true;
}

void VulkanEngine::cleanup() {
    if (mIsInitialized) {
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

