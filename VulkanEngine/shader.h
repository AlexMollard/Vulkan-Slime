#pragma once

#include "vulkan/vulkan.h"
#include <iostream>
#include <vector>

class shader {
public:
    static std::vector<char> readFile(const std::string &filename);

    static VkShaderModule createShaderModule(const std::vector<char> &code, const VkDevice &device);
};

