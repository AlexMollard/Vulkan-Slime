//
// Created by alexmollard on 6/1/22.
//

#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

struct AllocatedBufferUntyped {
    VkBuffer mBuffer{};
    VmaAllocation mAllocation{};
    VkDeviceSize mSize{0};

    VkDescriptorBufferInfo get_info(VkDeviceSize offset = 0) const;
};

template<typename T>
struct AllocatedBuffer : public AllocatedBufferUntyped {
    void operator=(const AllocatedBufferUntyped &other) {
        mBuffer = other.mBuffer;
        mAllocation = other.mAllocation;
        mSize = other.mSize;
    }

    explicit AllocatedBuffer(AllocatedBufferUntyped &other) {
        mBuffer = other.mBuffer;
        mAllocation = other.mAllocation;
        mSize = other.mSize;
    }

    AllocatedBuffer() = default;
};

struct AllocatedImage {
    VkImage mImage;
    VmaAllocation mAllocation;
    VkImageView mDefaultView;
    int mMipLevels;
};


inline VkDescriptorBufferInfo AllocatedBufferUntyped::get_info(VkDeviceSize offset) const {
    VkDescriptorBufferInfo info;
    info.buffer = mBuffer;
    info.offset = offset;
    info.range = mSize;
    return info;
}


enum class MeshpassType : uint8_t {
    None = 0,
    Forward = 1,
    Transparency = 2,
    DirectionalShadow = 3
};