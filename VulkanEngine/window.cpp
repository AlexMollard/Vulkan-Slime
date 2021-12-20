#include "window.h"
#include <iostream>
#include "debugging.h"

static void framebufferResizeCallback(GLFWwindow *win, [[maybe_unused]] int width, [[maybe_unused]] int height) {
    auto app = std::bit_cast<window*>(glfwGetWindowUserPointer(win));
    app->setFrameBufferResized(true);
}

void window::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mWindow = glfwCreateWindow(width, height, mWindowName, nullptr, nullptr);
    glfwSetWindowUserPointer(mWindow, this);
    glfwSetFramebufferSizeCallback(mWindow, framebufferResizeCallback);
}

void window::mainLoop() {
    mShouldClose = glfwWindowShouldClose(mWindow);
    glfwPollEvents();
}

void window::createSurface(VkSurfaceKHR &surface) {
    if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &surface) != VK_SUCCESS) {
        throw vulkanCreateError("Window Surface");
    }
    mSurface = surface;
}