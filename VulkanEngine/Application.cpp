#include "Application.h"
#include <map>

void Application::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	CleanUp();
}

void Application::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_window = glfwCreateWindow(WIDTH, HEIGHT, m_windowName, nullptr, nullptr);
}

void Application::InitVulkan()
{
	CreateInstance();
	m_ValidationLayer.SetUpDebugManager(m_instance);
	CreateSurface();
	m_DeviceAndQueue.PickPhysicalDevice(m_instance, m_surface, m_SwapChain);
	m_DeviceAndQueue.CreateLogicalDevice(m_ValidationLayer.enableValidationLayers, m_ValidationLayer.m_validationLayers);
	CreateSwapChain();
	CreateImageViews();
}

void Application::CreateInstance()
{
	if (m_ValidationLayer.enableValidationLayers && !m_ValidationLayer.CheckValidationLayerSupport())
		throw std::runtime_error("Validation layers requested, but not available!");

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
	auto extensions = m_ValidationLayer.GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

	// Validation Layers
	if (m_ValidationLayer.enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayer.m_validationLayers.size());
		createInfo.ppEnabledLayerNames = m_ValidationLayer.m_validationLayers.data();

		m_ValidationLayer.PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)& debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo,nullptr,&m_instance) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create instance!");
	}

	m_ValidationLayer.CheckExtensionSupport();
}

void Application::CreateSurface()
{
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}

void Application::CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_DeviceAndQueue.m_physicalDevice,m_surface);

	VkSurfaceFormatKHR surfaceFormat = m_SwapChain.ChooseSwapSurfaceFormat(swapChainSupport.m_formats);
	VkPresentModeKHR presentMode = m_SwapChain.ChooseSwapPresentMode(swapChainSupport.m_presentModes);
	VkExtent2D extent = m_SwapChain.ChooseSwapExtent(swapChainSupport.m_capabilities,*m_window);

	uint32_t imageCount = swapChainSupport.m_capabilities.minImageCount + 1;
	if (swapChainSupport.m_capabilities.maxImageCount > 0 && imageCount > swapChainSupport.m_capabilities.maxImageCount) {
		imageCount = swapChainSupport.m_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = m_DeviceAndQueue.FindQueueFamilies(m_DeviceAndQueue.m_physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.m_graphicsFamily.value(), indices.m_presentFamily.value() };

	if (indices.m_graphicsFamily != indices.m_presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.m_capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_DeviceAndQueue.m_device, &createInfo, nullptr, &m_SwapChain.swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(m_DeviceAndQueue.m_device, m_SwapChain.swapChain, &imageCount, nullptr);
	m_SwapChain.swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_DeviceAndQueue.m_device, m_SwapChain.swapChain, &imageCount, m_SwapChain.swapChainImages.data());

	m_SwapChain.swapChainImageFormat = surfaceFormat.format;
	m_SwapChain.swapChainExtent = extent;
}

void Application::CreateImageViews()
{
	m_SwapChain.swapChainImageViews.resize(m_SwapChain.swapChainImages.size());

	for (size_t i = 0; i < m_SwapChain.swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_SwapChain.swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_SwapChain.swapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_DeviceAndQueue.m_device, &createInfo, nullptr, &m_SwapChain.swapChainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image views!");
		}
	}
}

void Application::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
	}
}

void Application::CleanUp()
{
	for (auto imageView : m_SwapChain.swapChainImageViews) {
		vkDestroyImageView(m_DeviceAndQueue.m_device, imageView, nullptr);
	}

	vkDestroyDevice(m_DeviceAndQueue.m_device, nullptr);

	if (m_ValidationLayer.enableValidationLayers)
		m_ValidationLayer.DestroyDebugUtilsMessengerEXT(m_instance, m_ValidationLayer.m_debugMessenger, nullptr);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);

	glfwDestroyWindow(m_window);

	glfwTerminate();
}