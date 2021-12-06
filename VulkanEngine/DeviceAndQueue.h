#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "SwapChain.h"

struct QueueFamilyIndices
{
	std::optional<uint32_t> m_graphicsFamily;
	std::optional<uint32_t> m_presentFamily;

	bool IsComplete()
	{
		return (m_graphicsFamily.has_value() && m_presentFamily.has_value());
	};
};

struct DeviceAndQueue
{
	// devices and queues
	void PickPhysicalDevice(VkInstance& instance, VkSurfaceKHR& surface, SwapChain& swapChain);
	int GetDeviceSuitability(VkPhysicalDevice device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	void CreateLogicalDevice(const bool& enableValidationLayers, const std::vector<const char*>& validationLayers);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	bool IsDeviceSuitable(VkPhysicalDevice device);

	// Devices and queues Variables
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkSurfaceKHR* m_surface;
	SwapChain* m_swapChain;
};

