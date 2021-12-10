#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

class Window
{
public:
	void InitWindow();
	void MainLoop();

	VkInstance& Instance() { return m_instance; };
	GLFWwindow* Get() { return m_window; };

	bool ShouldClose() const { return m_ShouldClose; };

	bool GetFrameBufferResized() const { return m_frameBufferResized; };
	void SetFrameBufferResized(bool value) { m_frameBufferResized = value; };
private:
	// Window Variables
	VkInstance m_instance;
	GLFWwindow* m_window = nullptr;
	const char* m_windowName = "Vulkan Window";

	// Window Size
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

	bool m_ShouldClose;
	bool m_frameBufferResized = false;
};

