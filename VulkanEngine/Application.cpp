#include "Application.h"
#include <map>
#include "Debugging.h"
#include <array>

void Application::Run()
{
	m_window.InitWindow();
	InitVulkan();
	MainLoop();
	CleanUp();
}

void Application::InitVulkan()
{
	CreateInstance();
	m_validationLayer.SetUpDebugManager(m_window.Instance());
	m_window.CreateSurface(m_surface);
	
	m_deviceAndQueue.PickPhysicalDevice(m_window.Instance(), m_surface, m_swapChain);
	m_deviceAndQueue.CreateLogicalDevice(m_validationLayer.enableValidationLayers, m_validationLayer.m_validationLayers);
	m_swapChain.Init(m_deviceAndQueue.GetDevice(), m_deviceAndQueue, m_window);

	m_vertexBuffer.CreateBuffers(m_deviceAndQueue.GetDevice(), m_deviceAndQueue.GetPhysicalDevice(), m_swapChain.GetCommandPool(), m_deviceAndQueue.GetGraphicsQueue(),m_swapChain);

	//Descriptor pool and sets
	m_swapChain.CreateDescriptorPool(m_deviceAndQueue.GetDevice());
	m_swapChain.CreateDescriptorSets(m_deviceAndQueue.GetDevice(), m_vertexBuffer);

	m_swapChain.CreateCommandBuffers(m_deviceAndQueue.GetDevice(), m_vertexBuffer);
	m_renderer.CreateSyncObjects(m_deviceAndQueue.GetDevice(), m_swapChain);
}

void Application::MainLoop()
{
	while (!m_window.ShouldClose())
	{
		m_window.MainLoop();
		m_renderer.Draw(m_deviceAndQueue, m_swapChain, m_window, m_vertexBuffer);
	}

	vkDeviceWaitIdle(m_deviceAndQueue.GetDevice());
}

void Application::CreateInstance()
{
	if (m_validationLayer.enableValidationLayers && !m_validationLayer.CheckValidationLayerSupport())
		throw VulkanError("Validation layers requested, but not available!");

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
	auto extensions = m_validationLayer.GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

	// Validation Layers
	if (m_validationLayer.enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayer.m_validationLayers.size());
		createInfo.ppEnabledLayerNames = m_validationLayer.m_validationLayers.data();

		m_validationLayer.PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo,nullptr,&m_window.Instance()) != VK_SUCCESS)
	{
		throw VulkanCreateError("vulkan Instance");
	}

	m_validationLayer.CheckExtensionSupport();
}

void Application::CleanUp()
{
	auto& device = m_deviceAndQueue.GetDevice();

	m_swapChain.CleanUp(device, m_vertexBuffer);

	vkDestroyDescriptorSetLayout(device, m_swapChain.GetDescriptorSetLayout(), nullptr);

	m_vertexBuffer.CleanUp(device,m_swapChain.GetSwapChainImages());

	m_renderer.CleanUp(device);

	vkDestroyCommandPool(device, m_swapChain.GetCommandPool(), nullptr);

	vkDestroyDevice(device, nullptr);

	if (m_validationLayer.enableValidationLayers)
		m_validationLayer.DestroyDebugUtilsMessengerEXT(m_window.Instance(), m_validationLayer.m_debugMessenger, nullptr);

	vkDestroySurfaceKHR(m_window.Instance(), m_surface, nullptr);
	vkDestroyInstance(m_window.Instance(), nullptr);

	glfwDestroyWindow(m_window.Get());

	glfwTerminate();
}