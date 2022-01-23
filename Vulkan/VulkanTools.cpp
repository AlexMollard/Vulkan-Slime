//
// Created by alexm on 23/01/2022.
//

#include "VulkanTools.h"

namespace vkslime::tools {
    std::string physicalDeviceTypeString(VkPhysicalDeviceType type) {
        switch (type) {
#define STR(r) case VK_PHYSICAL_DEVICE_TYPE_ ##r: return #r
            STR(OTHER);
            STR(INTEGRATED_GPU);
            STR(DISCRETE_GPU);
            STR(VIRTUAL_GPU);
            STR(CPU);
#undef STR
            default:
                return "UNKNOWN_DEVICE_TYPE";
        }
    }


    bool fileExists(const std::string &filename) {
        std::ifstream f(filename.c_str());
        return !f.fail();
    }

    uint32_t alignedSize(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

}
