#include "Shader.h"
#include "Debugging.h"
#include <fstream>

std::vector<char> Shader::ReadFile(const std::string& filename)
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

VkShaderModule Shader::CreateShaderModule(const std::vector<char>& code, const VkDevice& device)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = std::bit_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw VulkanCreateError("Shader module!");
	}

	return shaderModule;
}