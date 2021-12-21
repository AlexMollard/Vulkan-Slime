#pragma once

#include "validationLayer.h"
#include "swapChain.h"
#include "deviceAndQueue.h"
#include "shader.h"
#include "rendering.h"
#include "image.h"

class application {
public:
    void run();

private:
    void initVulkan();

    void mainLoop();

    void cleanUp();

    // Vulkan
    void createInstance();

    window mWindow;
    VkSurfaceKHR mSurface = nullptr;

    validationLayer mValidationLayer;
    deviceAndQueue mDeviceAndQueue;
    swapChain mSwapChain;
    vertexInput mVertexBuffer;
    rendering mRenderer;
    image mImage;

    mesh spear;
};