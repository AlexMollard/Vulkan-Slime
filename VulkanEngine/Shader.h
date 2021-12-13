#pragma once
#include "vulkan/vulkan.h"
#include <iostream>
#include <vector>

class Shader
{
public:
	static std::vector<char> ReadFile(const std::string& filename);
	static VkShaderModule CreateShaderModule(const std::vector<char>& code, const VkDevice& device);
};

