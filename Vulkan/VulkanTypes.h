//
// Created by alexmollard on 6/1/22.
//

#pragma once

#include <vk_mem_alloc.h>

namespace VulkanType {
    struct AllocatedBuffer {
        VkBuffer _buffer;
        VmaAllocation _allocation;
    };

    struct AllocatedImage {
        VkImage _image;
        VmaAllocation _allocation;
    };

};
