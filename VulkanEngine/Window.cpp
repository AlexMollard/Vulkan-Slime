#include "Window.h"
#include <iostream>
#include "Debugging.h"

static void FramebufferResizeCallback(GLFWwindow* window, [[maybe_unused]] int width, [[maybe_unused]] int height)
{
	auto app = std::bit_cast<Window*>(glfwGetWindowUserPointer(window));
	app->SetFrameBufferResized(true);
}

void Window::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(WIDTH, HEIGHT, m_windowName, nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
}

void Window::MainLoop()
{
	m_ShouldClose = glfwWindowShouldClose(m_window);
	glfwPollEvents();
}

void Window::CreateSurface(VkSurfaceKHR& surface)
{
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface) != VK_SUCCESS) {
		throw VulkanCreateError("Window Surface");
	}
	m_surface = surface;
}