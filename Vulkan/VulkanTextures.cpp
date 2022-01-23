//
// Created by alexm on 10/01/2022.
//

#include "VulkanTextures.h"
#include <iostream>

#include "VulkanInitializers.h"
#include "Tracy.hpp"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include "texture_asset.h"
#include "asset_loader.h"
#include <SDL_filesystem.h>

#include "Log.h"

bool vkutil::load_image_from_file(VulkanEngine &engine, const char *file, AllocatedImage &outImage) {
    int texWidth, texHeight, texChannels;

    stbi_uc *pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        Log::error("Failed to load texture file " + std::string(file));
        return false;
    }

    void *pixel_ptr = pixels;
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

    AllocatedBufferUntyped stagingBuffer = engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                VMA_MEMORY_USAGE_CPU_ONLY);

    void *data;
    vmaMapMemory(engine.mAllocator, stagingBuffer.mAllocation, &data);

    memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));

    vmaUnmapMemory(engine.mAllocator, stagingBuffer.mAllocation);

    stbi_image_free(pixels);


    outImage = upload_image(texWidth, texHeight, image_format, engine, stagingBuffer);


    vmaDestroyBuffer(engine.mAllocator, stagingBuffer.mBuffer, stagingBuffer.mAllocation);

    Log::trace("Texture loaded succesfully: " + std::string(file));


    return true;
}


bool vkutil::load_image_from_asset(VulkanEngine &engine, const char *filename, AllocatedImage &outImage) {
    assets::AssetFile file;
    bool loaded = assets::load_binaryfile(filename, file);

    if (!loaded) {
        std::cout << "Error when loading texture " << filename << std::endl;
        return false;
    }

    assets::TextureInfo textureInfo = assets::read_texture_info(&file);


    VkDeviceSize imageSize = textureInfo.textureSize;
    VkFormat image_format;
    switch (textureInfo.textureFormat) {
        case assets::TextureFormat::RGBA8:
            image_format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        default:
            return false;
    }

    AllocatedBufferUntyped stagingBuffer = engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                VMA_MEMORY_USAGE_UNKNOWN,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    std::vector<MipmapInfo> mips;

    void *data;
    vmaMapMemory(engine.mAllocator, stagingBuffer.mAllocation, &data);
    size_t offset = 0;
    {

        for (int i = 0; i < textureInfo.pages.size(); i++) {
            ZoneScopedNC("Unpack Texture", tracy::Color::Magenta)
            MipmapInfo mip{};
            mip.dataOffset = offset;
            mip.dataSize = textureInfo.pages[i].originalSize;
            mips.push_back(mip);
            assets::unpack_texture_page(&textureInfo, i, file.binaryBlob.data(), (char *) data + offset);

            offset += mip.dataSize;
        }
    }
    vmaUnmapMemory(engine.mAllocator, stagingBuffer.mAllocation);

    outImage = upload_image_mipmapped(textureInfo.pages[0].width, textureInfo.pages[0].height, image_format, engine,
                                      stagingBuffer, mips);

    vmaDestroyBuffer(engine.mAllocator, stagingBuffer.mBuffer, stagingBuffer.mAllocation);

    return true;
}

AllocatedImage vkutil::upload_image(int texWidth, int texHeight, VkFormat image_format, VulkanEngine &engine,
                                    AllocatedBufferUntyped &stagingBuffer) {
    VkExtent3D imageExtent;
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = vkslime::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT |
                                                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                             imageExtent);

    AllocatedImage newImage{};

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //allocate and create the image
    vmaCreateImage(engine.mAllocator, &dimg_info, &dimg_allocinfo, &newImage.mImage, &newImage.mAllocation, nullptr);

    //transition image to transfer-receiver
    engine.immediate_submit([&](VkCommandBuffer cmd) {
        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier imageBarrier_toTransfer = {};
        imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier_toTransfer.image = newImage.mImage;
        imageBarrier_toTransfer.subresourceRange = range;

        imageBarrier_toTransfer.srcAccessMask = 0;
        imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        //barrier the image into the transfer-receive layout
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &imageBarrier_toTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = imageExtent;

        //copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, stagingBuffer.mBuffer, newImage.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

        imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        //barrier the image into the shader readable layout
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &imageBarrier_toReadable);
    });


    //build a default imageview
    VkImageViewCreateInfo view_info = vkslime::imageview_create_info(image_format, newImage.mImage,
                                                                     VK_IMAGE_ASPECT_COLOR_BIT);

    vkCreateImageView(engine.mDevice, &view_info, nullptr, &newImage.mDefaultView);


    engine.mMainDeletionQueue.push_function([=, &engine]() {

        vmaDestroyImage(engine.mAllocator, newImage.mImage, newImage.mAllocation);
    });


    newImage.mMipLevels = 1;// mips.size();
    return newImage;
}

AllocatedImage vkutil::upload_image_mipmapped(int texWidth, int texHeight, VkFormat image_format, VulkanEngine &engine,
                                              AllocatedBufferUntyped &stagingBuffer, std::vector<MipmapInfo> mips) {
    VkExtent3D imageExtent;
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = vkslime::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT |
                                                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                             imageExtent);

    dimg_info.mipLevels = (uint32_t) mips.size();

    AllocatedImage newImage{};

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //allocate and create the image
    vmaCreateImage(engine.mAllocator, &dimg_info, &dimg_allocinfo, &newImage.mImage, &newImage.mAllocation, nullptr);

    //transition image to transfer-receiver
    engine.immediate_submit([&](VkCommandBuffer cmd) {
        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = (uint32_t) mips.size();
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier imageBarrier_toTransfer = {};
        imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier_toTransfer.image = newImage.mImage;
        imageBarrier_toTransfer.subresourceRange = range;

        imageBarrier_toTransfer.srcAccessMask = 0;
        imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        //barrier the image into the transfer-receive layout
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &imageBarrier_toTransfer);

        for (int i = 0; i < mips.size(); i++) {


            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = mips[i].dataOffset;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = i;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = imageExtent;

            //copy the buffer into the image
            vkCmdCopyBufferToImage(cmd, stagingBuffer.mBuffer, newImage.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &copyRegion);

            imageExtent.width /= 2;
            imageExtent.height /= 2;
        }
        VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

        imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        //barrier the image into the shader readable layout
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &imageBarrier_toReadable);
    });


    newImage.mMipLevels = (uint32_t) mips.size();


    //build a default imageview
    VkImageViewCreateInfo view_info = vkslime::imageview_create_info(image_format, newImage.mImage,
                                                                     VK_IMAGE_ASPECT_COLOR_BIT);
    view_info.subresourceRange.levelCount = newImage.mMipLevels;
    vkCreateImageView(engine.mDevice, &view_info, nullptr, &newImage.mDefaultView);

    engine.mMainDeletionQueue.push_function([=, &engine]() {

        vmaDestroyImage(engine.mAllocator, newImage.mImage, newImage.mAllocation);
    });

    return newImage;
}

