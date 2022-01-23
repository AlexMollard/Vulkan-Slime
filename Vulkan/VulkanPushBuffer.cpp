//
// Created by alexm on 23/01/2022.
//

#include "VulkanPushBuffer.h"
#include <cstring>

uint32_t vkutil::PushBuffer::push(void* data, size_t size)
{
    uint32_t offset = currentOffset;
    char* target = (char*)mapped;
    target += currentOffset;
    memcpy(target, data, size);
    currentOffset += static_cast<uint32_t>(size);
    currentOffset = pad_uniform_buffer_size(currentOffset);

    return offset;
}

void vkutil::PushBuffer::init(VmaAllocator& allocator, AllocatedBufferUntyped sourceBuffer, uint32_t alignement)
{
    align = alignement;
    source = sourceBuffer;
    currentOffset = 0;
    vmaMapMemory(allocator, sourceBuffer.mAllocation, &mapped);
}

void vkutil::PushBuffer::reset()
{
    currentOffset = 0;
}

uint32_t vkutil::PushBuffer::pad_uniform_buffer_size(uint32_t originalSize) const
{
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = align;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return static_cast<uint32_t>(alignedSize);
}