#pragma once
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

class window {
public:
    void initWindow();

    void mainLoop();

    VkInstance &instance() { return mInstance; };

    GLFWwindow *get() { return mWindow; };

    VkSurfaceKHR &getSurface() { return mSurface; };

    [[nodiscard]] bool shouldClose() const { return mShouldClose; };

    [[nodiscard]] bool getFrameBufferResized() const { return mFrameBufferResized; };

    void setFrameBufferResized(bool value) { mFrameBufferResized = value; };

    void createSurface(VkSurfaceKHR &surface);

private:
    VkSurfaceKHR mSurface{};

    // Window Variables
    VkInstance mInstance{};
    GLFWwindow *mWindow = nullptr;
    const char *mWindowName = "Vulkan Window";

    // Window Size
    const int width = 800;
    const int height = 600;

    bool mShouldClose = false;
    bool mFrameBufferResized = false;
};