#pragma once
#include "ValidationLayer.h"
#include "DeviceAndQueue.h"
#include "SwapChain.h"

class Application
{
public:
	void Run();

private:
	void InitWindow();
	void InitVulkan();
	void MainLoop();
	void CleanUp();

	// Vulkan
	void CreateInstance();
	void CreateSurface();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateGraphicsPipeline();
	void CreateRenderPass();
	void CreateFrameBuffer();

	static std::vector<char> ReadFile(const std::string& filename);
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	// Window Variables
	VkInstance m_instance;
	GLFWwindow* m_window = nullptr;
	const char* m_windowName = "Vulkan Window";
	
	// Window Size
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

	ValidationLayer m_ValidationLayer;
	DeviceAndQueue m_DeviceAndQueue;
	SwapChain m_SwapChain;
	VkSurfaceKHR m_Surface;
	VkPipelineLayout m_PipelineLayout;
	VkRenderPass m_RenderPass;
	VkPipeline m_GraphicsPipeline;
	std::vector<VkFramebuffer> m_SwapChainFramebuffers;
};

