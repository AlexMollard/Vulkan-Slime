#pragma once
#include "ValidationLayer.h"
#include "SwapChain.h"
#include "DeviceAndQueue.h"
#include "Shader.h"
#include "Rendering.h"
#include "Image.h"

class Application
{
public:
	void Run();

private:
	void InitVulkan();
	void MainLoop();
	void CleanUp();

	// Vulkan
	void CreateInstance();

	Window m_window;
	VkSurfaceKHR m_surface = nullptr;

	ValidationLayer m_validationLayer;
	DeviceAndQueue m_deviceAndQueue;
	SwapChain m_swapChain;
	Shader m_mainShader;
	VertexInput m_vertexBuffer;
	Rendering m_renderer;
	Image m_image;
};