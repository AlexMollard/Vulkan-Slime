//
// Created by alexm on 23/01/2022.
//

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanInitializers.h"

#include <cassert>
#include <string>
#include <cstring>
#include <fstream>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <fstream>

namespace vkslime::tools {
    std::string errorString(VkResult errorCode);

    std::string physicalDeviceTypeString(VkPhysicalDeviceType type);

    bool fileExists(const std::string &filename);

    uint32_t alignedSize(uint32_t value, uint32_t alignment);
}

inline std::string vkslime::tools::errorString(VkResult errorCode) {
    switch (errorCode) {
#define STR(r) case VK_ ##r: return #r
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
#undef STR
        default:
            return "UNKNOWN_ERROR";
    }
}


inline void VK_CHECK_RESULT(const VkResult& x)
{
        VkResult res = (x);
        if (res != VK_SUCCESS)
        {
            std::cout << "Fatal : VkResult is \"" << vkslime::tools::errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n";
            assert(res == VK_SUCCESS);
            abort();
        }
}
