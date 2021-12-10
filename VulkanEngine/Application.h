#pragma once
#include "ValidationLayer.h"
#include "DeviceAndQueue.h"
#include "SwapChain.h"
#include "Window.h"

class Application
{
public:
	void Run();

private:
	void InitVulkan();
	void MainLoop();
	void DrawFrame();
	void CleanUp();

	// Vulkan
	void CreateInstance();
	void CreateSurface();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateGraphicsPipeline();
	void CreateRenderPass();
	void CreateFrameBuffer();

	void CreateCommandPool();
	void CreateCommandBuffers();

	void CreateSyncObjects();

	void RecreateSwapChain();
	void CleanUpSwapChain();


	static std::vector<char> ReadFile(const std::string& filename);
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	Window m_window;

	ValidationLayer m_validationLayer;
	DeviceAndQueue m_deviceAndQueue;
	SwapChain m_swapChain;

	VkSurfaceKHR m_surface;
	VkPipelineLayout m_pipelineLayout;
	VkRenderPass m_renderPass;
	VkPipeline m_graphicsPipeline;
	std::vector<VkFramebuffer> m_swapChainFramebuffers;
	VkCommandPool m_commandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;

	const int MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;
	std::vector<VkFence> m_imagesInFlight;
	size_t m_currentFrame = 0;
};