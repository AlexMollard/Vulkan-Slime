#include "Application.h"
#include <map>
#include <fstream>
#include "Debugging.h"
#include <array>

void Application::Run()
{
	m_window.InitWindow();
	InitVulkan();
	MainLoop();
	CleanUp();
}

void Application::InitVulkan()
{
	CreateInstance();
	m_validationLayer.SetUpDebugManager(m_window.Instance());
	CreateSurface();
	m_deviceAndQueue.PickPhysicalDevice(m_window.Instance(), m_surface, m_swapChain);
	m_deviceAndQueue.CreateLogicalDevice(m_validationLayer.enableValidationLayers, m_validationLayer.m_validationLayers);
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFrameBuffer();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateSyncObjects();
}

void Application::MainLoop()
{
	while (!m_window.ShouldClose())
	{
		m_window.MainLoop();
		DrawFrame();
	}

	vkDeviceWaitIdle(m_deviceAndQueue.m_device);
}

void Application::DrawFrame()
{
	auto& device = m_deviceAndQueue.m_device;

	vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device, m_swapChain.swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw VulkanError("failed to acquire swap chain image!");
	}

	if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}
	m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];


	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::vector<VkSemaphore> waitSemaphores = {m_imageAvailableSemaphores[m_currentFrame]};
	std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages.data();

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

	std::vector < VkSemaphore> signalSemaphores = { m_renderFinishedSemaphores[m_currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

	if (vkQueueSubmit(m_deviceAndQueue.m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
		throw VulkanError("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores.data();

	std::vector<VkSwapchainKHR> swapChains = { m_swapChain.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains.data();

	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(m_deviceAndQueue.m_presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.GetFrameBufferResized()) {
		m_window.SetFrameBufferResized(false);
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw VulkanError("failed to present swap chain image!");
	}

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::CreateInstance()
{
	if (m_validationLayer.enableValidationLayers && !m_validationLayer.CheckValidationLayerSupport())
		throw VulkanError("Validation layers requested, but not available!");

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Extensions
	auto extensions = m_validationLayer.GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

	// Validation Layers
	if (m_validationLayer.enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayer.m_validationLayers.size());
		createInfo.ppEnabledLayerNames = m_validationLayer.m_validationLayers.data();

		m_validationLayer.PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo,nullptr,&m_window.Instance()) != VK_SUCCESS)
	{
		throw VulkanCreateError("vulkan Instance");
	}

	m_validationLayer.CheckExtensionSupport();
}

void Application::CreateSurface()
{
	if (glfwCreateWindowSurface(m_window.Instance(), m_window.Get(), nullptr, &m_surface) != VK_SUCCESS) {
		throw VulkanCreateError("Window Surface");
	}
}

void Application::CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_deviceAndQueue.m_physicalDevice,m_surface);

	VkSurfaceFormatKHR surfaceFormat = m_swapChain.ChooseSwapSurfaceFormat(swapChainSupport.m_formats);
	VkPresentModeKHR presentMode = m_swapChain.ChooseSwapPresentMode(swapChainSupport.m_presentModes);
	VkExtent2D extent = m_swapChain.ChooseSwapExtent(swapChainSupport.m_capabilities,*m_window.Get());

	uint32_t imageCount = swapChainSupport.m_capabilities.minImageCount + 1;
	if (swapChainSupport.m_capabilities.maxImageCount > 0 && imageCount > swapChainSupport.m_capabilities.maxImageCount) {
		imageCount = swapChainSupport.m_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = m_deviceAndQueue.FindQueueFamilies(m_deviceAndQueue.m_physicalDevice);
	std::vector<uint32_t> queueFamilyIndices = { indices.m_graphicsFamily.value(), indices.m_presentFamily.value() };

	if (indices.m_graphicsFamily != indices.m_presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	}
	else 
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.m_capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_deviceAndQueue.m_device, &createInfo, nullptr, &m_swapChain.swapChain) != VK_SUCCESS) {
		throw VulkanCreateError("Swap Chain");
	}

	vkGetSwapchainImagesKHR(m_deviceAndQueue.m_device, m_swapChain.swapChain, &imageCount, nullptr);
	m_swapChain.swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_deviceAndQueue.m_device, m_swapChain.swapChain, &imageCount, m_swapChain.swapChainImages.data());

	m_swapChain.swapChainImageFormat = surfaceFormat.format;
	m_swapChain.swapChainExtent = extent;
}

void Application::CreateImageViews()
{
	m_swapChain.swapChainImageViews.resize(m_swapChain.swapChainImages.size());

	for (size_t i = 0; i < m_swapChain.swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swapChain.swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_swapChain.swapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_deviceAndQueue.m_device, &createInfo, nullptr, &m_swapChain.swapChainImageViews[i]) != VK_SUCCESS) {
			throw VulkanCreateError("Image views");
		}
	}
}

void Application::CreateGraphicsPipeline()
{
	// Shader Stuff
	auto vertShaderCode = ReadFile("../Shaders/vert.spv");
	auto fragShaderCode = ReadFile("../Shaders/frag.spv");

	VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_swapChain.swapChainExtent.width;
	viewport.height = (float)m_swapChain.swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_swapChain.swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(m_deviceAndQueue.m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
		throw VulkanCreateError("Pipeline layout");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(m_deviceAndQueue.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) 
	{
		throw VulkanCreateError("Graphics Pipeline");
	}

	vkDestroyShaderModule(m_deviceAndQueue.m_device, fragShaderModule, nullptr);
	vkDestroyShaderModule(m_deviceAndQueue.m_device, vertShaderModule, nullptr);
}

void Application::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapChain.swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

	// Colour attachment
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	// Depth attachment
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(m_deviceAndQueue.m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
		throw VulkanCreateError("Render Pass");
	}
}

void Application::CreateFrameBuffer()
{
	const auto& swapChainImageViews = m_swapChain.swapChainImageViews;
	const auto& swapChainExtent = m_swapChain.swapChainExtent;

	m_swapChainFramebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++) 
	{
		std::vector<VkImageView> attachments = {swapChainImageViews[i]};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_deviceAndQueue.m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw VulkanCreateError("Framebuffer");
		}
	}
}

void Application::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = m_deviceAndQueue.FindQueueFamilies(m_deviceAndQueue.m_physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.m_graphicsFamily.value();
	poolInfo.flags = 0; // Optional

	if (vkCreateCommandPool(m_deviceAndQueue.m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
		throw VulkanCreateError("Command pool");
	}
}

void Application::CreateCommandBuffers()
{
	m_commandBuffers.resize(m_swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

	if (vkAllocateCommandBuffers(m_deviceAndQueue.m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
		throw VulkanError("failed to allocate command buffers!");
	}

	for (size_t i = 0; i < m_commandBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo) != VK_SUCCESS) {
			throw VulkanError("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.framebuffer = m_swapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapChain.swapChainExtent;

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

		vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(m_commandBuffers[i]);

		if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS) {
			throw VulkanError("failed to record command buffer!");
		}
	}
}

void Application::CreateSyncObjects()
{
	m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	m_imagesInFlight.resize(m_swapChain.swapChainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(m_deviceAndQueue.m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_deviceAndQueue.m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_deviceAndQueue.m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
			throw VulkanCreateError("Synchronization objects for a frame");
		}
	}
}

void Application::RecreateSwapChain()
{

	int width = 0;
	int	height = 0;
	glfwGetFramebufferSize(m_window.Get(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(m_window.Get(), &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_deviceAndQueue.m_device);

	CleanUpSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFrameBuffer();
	CreateCommandBuffers();
}

void Application::CleanUpSwapChain()
{
	for (auto framebuffer : m_swapChainFramebuffers) {
		vkDestroyFramebuffer(m_deviceAndQueue.m_device, framebuffer, nullptr);
	}

	vkFreeCommandBuffers(m_deviceAndQueue.m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

	vkDestroyPipeline(m_deviceAndQueue.m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_deviceAndQueue.m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_deviceAndQueue.m_device, m_renderPass, nullptr);

	for (auto imageView : m_swapChain.swapChainImageViews) {
		vkDestroyImageView(m_deviceAndQueue.m_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_deviceAndQueue.m_device, m_swapChain.swapChain, nullptr);
}

std::vector<char> Application::ReadFile(const std::string& filename)
{
	auto file = std::ifstream(filename, std::ios::ate | std::ios::binary);
	
	if (!file.is_open())
	{
		throw CppError("failed to open file!");
	}

	auto fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

VkShaderModule Application::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = std::bit_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_deviceAndQueue.m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw VulkanCreateError("Shader module!");
	}

	return shaderModule;
}

void Application::CleanUp()
{
	CleanUpSwapChain();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(m_deviceAndQueue.m_device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(m_deviceAndQueue.m_device, m_imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(m_deviceAndQueue.m_device, m_inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(m_deviceAndQueue.m_device, m_commandPool, nullptr);

	vkDestroyDevice(m_deviceAndQueue.m_device, nullptr);

	if (m_validationLayer.enableValidationLayers)
		m_validationLayer.DestroyDebugUtilsMessengerEXT(m_window.Instance(), m_validationLayer.m_debugMessenger, nullptr);

	vkDestroySurfaceKHR(m_window.Instance(), m_surface, nullptr);
	vkDestroyInstance(m_window.Instance(), nullptr);

	glfwDestroyWindow(m_window.Get());

	glfwTerminate();
}