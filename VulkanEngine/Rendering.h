#pragma once
#include "vulkan/vulkan.h"
#include <vector>

class DeviceAndQueue;
class SwapChain;
class Window;
class VertexInput;

class Rendering
{
public:
	void Draw(DeviceAndQueue& deviceAndQueue, SwapChain& swapChain, Window& window, VertexInput& vertexBuffer);
	void CreateSyncObjects(const VkDevice& device, SwapChain& swapChain);
	void CleanUp(VkDevice& device);
private:
	void UpdateUniform(uint32_t currentImage, VkDevice& device, SwapChain& swapchain, VertexInput& vertexInput);

	const int MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;
	std::vector<VkFence> m_imagesInFlight;
	size_t m_currentFrame = 0;
};

