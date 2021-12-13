#pragma once
#include "Window.h"
#include "VertexInput.h"

#include <optional>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <iostream>
#include <set>

class DeviceAndQueue;

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR m_capabilities;
	std::vector<VkSurfaceFormatKHR> m_formats;
	std::vector<VkPresentModeKHR> m_presentModes;
};

class SwapChain
{
public:
	void Init(const VkDevice& device, DeviceAndQueue& deviceAndQueue, Window& window);

	void CreateSwapChain(DeviceAndQueue& deviceAndQueue, Window& window);
	void RecreateSwapChain(DeviceAndQueue& deviceAndQueue, Window& window, VertexInput& vertexBuffer);
	void CreateImageViews(const VkDevice& device);
	void CreateRenderPass(const VkDevice& device);
	void CreateGraphicsPipeline(const VkDevice& device);
	void CreateFrameBuffer(const VkDevice& device);
	void CreateCommandBuffers(const VkDevice& device, VertexInput& vertexBuffer);
	void CreateCommandPool(DeviceAndQueue& deviceAndQueue);
	void CreateDescriptorPool(VkDevice& device);
	void CreateDescriptorSets(VkDevice& device, VertexInput& vertexInput);

	VkDescriptorSetLayout& GetDescriptorSetLayout() { return m_descriptorSetLayout; };

	void CleanUp(const VkDevice& device, VertexInput& vertexInput);

	VkSwapchainKHR& GetSwapChain() { return m_swapChain; };
	std::vector<VkImage>& GetSwapChainImages() { return m_swapChainImages; };

	VkCommandPool& GetCommandPool() { return m_commandPool; };
	std::vector<VkCommandBuffer>& GetCommandBuffers() { return m_commandBuffers; };

	const std::vector<const char*>& GetDeviceExtensions() const { return m_deviceExtensions; };

	VkExtent2D& GetSwapChainExtent() {return m_swapChainExtent ;};
	
	static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR& surface);
private:
	void CreateDescriptorSetLayout(const VkDevice& device);

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow& window);

	VkSurfaceKHR* m_surface;
	VkSwapchainKHR m_swapChain;
	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;

	VkPipelineLayout m_pipelineLayout;
	VkRenderPass m_renderPass;
	VkPipeline m_graphicsPipeline;
	std::vector<VkFramebuffer> m_swapChainFramebuffers;
	VkCommandPool m_commandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;

	const std::vector<const char*> m_deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
};

