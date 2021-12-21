#include "application.h"
#include "debugging.h"
#include "mesh.h"
#include <array>

void application::run() {
    mWindow.initWindow();
    initVulkan();
    mainLoop();
    cleanUp();
}

void application::initVulkan() {
    createInstance();
    mValidationLayer.init(mWindow.instance());
    mWindow.createSurface(mSurface);

    mDeviceAndQueue.pickPhysicalDevice(mWindow.instance(), mSurface, mSwapChain);
    auto &device = mDeviceAndQueue.getDevice();

    mDeviceAndQueue.createLogicalDevice(mValidationLayer.enableValidationLayers,
                                        mValidationLayer.mValidationLayers);

    mSwapChain.init(device, mDeviceAndQueue, mWindow);

    std::string imageDir = "../Models/soulspear_diffuse.tga";
    mImage.create(device, mDeviceAndQueue.getPhysicalDevice(), mSwapChain.getCommandPool(),
                  mDeviceAndQueue.getGraphicsQueue(), imageDir);
    mImage.createView(device, mSwapChain);
    mImage.createSampler(device, mDeviceAndQueue.getPhysicalDevice());

    mVertexBuffer.createUniformBuffer(mDeviceAndQueue, mSwapChain);

    //Descriptor pool and sets
    mSwapChain.createDescriptorPool(device);
    mSwapChain.createDescriptorSets(device, mVertexBuffer.getUniformBuffers(), mImage);

    spear.init(mDeviceAndQueue, mSwapChain.getCommandPool(), "../Models/soulspear.obj");


    mSwapChain.createCommandBuffers(device, spear);
    mRenderer.createSyncObjects(device, mSwapChain);
}

void application::mainLoop() {
    while (!mWindow.shouldClose()) {
        mWindow.mainLoop();
        mSwapChain.createCommandBuffers(mDeviceAndQueue.getDevice(), spear);
        mRenderer.draw(mDeviceAndQueue, mSwapChain, mWindow, mVertexBuffer, mImage);
    }

    vkDeviceWaitIdle(mDeviceAndQueue.getDevice());
}

void application::createInstance() {
    if (mValidationLayer.enableValidationLayers && !mValidationLayer.checkValidationLayerSupport())
        throw vulkanError("Validation layers requested, but not available!");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Extensions
    auto extensions = mValidationLayer.getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    // Validation Layers
    if (mValidationLayer.enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(mValidationLayer.mValidationLayers.size());
        createInfo.ppEnabledLayerNames = mValidationLayer.mValidationLayers.data();

        validationLayer::populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &mWindow.instance()) != VK_SUCCESS) {
        throw vulkanCreateError("vulkan instance");
    }

    validationLayer::checkExtensionSupport();
}

void application::cleanUp() {
    auto &device = mDeviceAndQueue.getDevice();

    mSwapChain.cleanUp(device, mVertexBuffer);

    mImage.cleanUp(device);

    vkDestroyDescriptorSetLayout(device, mSwapChain.getDescriptorSetLayout(), nullptr);

    spear.cleanUp(device);

    mRenderer.cleanUp(device);

    vkDestroyCommandPool(device, mSwapChain.getCommandPool(), nullptr);

    vkDestroyDevice(device, nullptr);

    if (mValidationLayer.enableValidationLayers)
        validationLayer::destroyDebugUtilsMessengerExt(mWindow.instance(), mValidationLayer.mDebugMessenger,
                                                       nullptr);

    vkDestroySurfaceKHR(mWindow.instance(), mSurface, nullptr);
    vkDestroyInstance(mWindow.instance(), nullptr);

    glfwDestroyWindow(mWindow.get());

    glfwTerminate();
}