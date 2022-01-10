//
// Created by alexmollard on 6/1/22.
//

#pragma once

#include <vk_mem_alloc.h>

namespace vktype {
    struct AllocatedBuffer {
        VkBuffer mBuffer;
        VmaAllocation mAllocation;
    };

    struct AllocatedImage {
        VkImage mImage;
        VmaAllocation mAllocation;
    };

};
