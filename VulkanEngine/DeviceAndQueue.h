#pragma once
#include "Debugging.h"
#include "vulkan/vulkan.h"

#include <optional>
#include <vector>
#include <map>
#include <iostream>
#include <set>

class SwapChain;

struct QueueFamilyIndices
{
	std::optional<uint32_t> m_graphicsFamily;
	std::optional<uint32_t> m_presentFamily;

	bool IsComplete() const
	{
		return (m_graphicsFamily.has_value() && m_presentFamily.has_value());
	};
};

class DeviceAndQueue
{
public:
	void PickPhysicalDevice(VkInstance& instance, VkSurfaceKHR& surface, SwapChain& swapChain);
	void CreateLogicalDevice(const bool& enableValidationLayers, const std::vector<const char*>& validationLayers);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;

	VkDevice& GetDevice() { return m_device; };
	VkPhysicalDevice& GetPhysicalDevice() { return m_physicalDevice; };

	VkQueue& GetGraphicsQueue() { return m_graphicsQueue; };
	VkQueue& GetPresentQueue() { return m_presentQueue; };
private:
	int GetDeviceSuitability(VkPhysicalDevice device) const;
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
	bool IsDeviceSuitable(const VkPhysicalDevice& device);

	void OutputExtension();

	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkSurfaceKHR* m_surface;
	SwapChain* m_swapChain;
};

