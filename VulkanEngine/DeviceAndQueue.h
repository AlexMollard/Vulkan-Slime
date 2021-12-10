#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "SwapChain.h"
#include "Debugging.h"

struct QueueFamilyIndices
{
	std::optional<uint32_t> m_graphicsFamily;
	std::optional<uint32_t> m_presentFamily;

	bool IsComplete() const
	{
		return (m_graphicsFamily.has_value() && m_presentFamily.has_value());
	};
};

struct DeviceAndQueue
{
	// devices and queues
	void PickPhysicalDevice(VkInstance& instance, VkSurfaceKHR& surface, SwapChain& swapChain);
	int GetDeviceSuitability(VkPhysicalDevice device) const;
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
	void CreateLogicalDevice(const bool& enableValidationLayers, const std::vector<const char*>& validationLayers);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
	bool IsDeviceSuitable(VkPhysicalDevice device);
	void OutputExtension();

	// Devices and queues Variables
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkSurfaceKHR* m_surface;
	SwapChain* m_swapChain;
};

