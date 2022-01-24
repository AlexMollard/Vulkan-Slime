//
// Created by alexm on 23/12/2021.
//
#include "VulkanEngine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "VulkanInitializers.h"
#include "VulkanDescriptors.h"
#include "VulkanTextures.h"
#include "VulkanShaders.h"
#include "VulkanProfiler.h"

#include "VkBootstrap.h"

#include "prefab_asset.h"
#include "material_asset.h"

#include <cmath>
#include <iostream>
#include <fstream>
#include <chrono>
#include <sstream>
#include <filesystem>
#include <algorithm>

#include <fmt/os.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "Tracy.hpp"
#include "TracyVulkan.hpp"

#include "Log.h"


void VulkanEngine::init() {
    auto start = std::chrono::steady_clock::now();
    ZoneScopedN("Engine Init")

    Log::init();

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);
    Log::info("SDL inited");

    SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN;

    //create blank SDL window for our application
    mWindow = SDL_CreateWindow(
            "Vulkan Slime", //window title
            SDL_WINDOWPOS_UNDEFINED, //window position x
            SDL_WINDOWPOS_UNDEFINED, //window position y
            (int) mWindowExtent.width,  //window width in pixels
            (int) mWindowExtent.height, //window height in pixels
            windowFlags
    );

    mMeshes.reserve(1000);

    init_vulkan();

    mProfiler = new vkutil::VulkanProfiler();

    mProfiler->init(mDevice, mGpuProperties.limits.timestampPeriod);

    mShaderCache.init(mDevice);

    mRenderScene.init();

    init_swapchain();


    init_forward_renderpass();
    init_copy_renderpass();
    init_shadow_renderpass();

    init_framebuffers();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_pipelines();

    Log::info("Engine Initialized, starting Load");

    load_images();

    load_meshes();

    init_scene();

    init_imgui();

    mRenderScene.build_batches();

    //mRenderScene.merge_meshes(this);
    //everything went fine
    mIsInitialized = true;

    mCamera = {};
    mCamera.position = { 0.f,6.f,5.f };

    mMainLight.lightPosition = { 0,0,0 };
    mMainLight.lightDirection = glm::vec3(0.3, -1, 0.3);
    mMainLight.shadowExtent = { 100 ,100 ,100 };

    auto end = std::chrono::steady_clock::now();
    auto time = std::chrono::duration<double>(end - start).count();
    Log::warn("StartUp Time: " + std::to_string(time) + " Seconds.");
}

void VulkanEngine::init_vulkan() {
    vkb::InstanceBuilder builder;

    //make the Vulkan instance, with basic debug features
    auto inst_ret =
            builder.set_app_name("Slime Vulkan")
                    .request_validation_layers(true)
                    .require_api_version(1, 2, 0)
                    .use_default_debug_messenger()
                    .build();

    Log::info("Vulkan Instance initialized");

    vkb::Instance vkb_inst = inst_ret.value();

    //store the instance
    mInstance = vkb_inst.instance;

    // get the surface of the window we opened with SDL
    SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

    Log::info("SDL Surface initialized");

    //use vkbootstrap to select a GPU.
    //We want a GPU that can write to the SDL surface and supports Vulkan 1.2
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    VkPhysicalDeviceFeatures feats{};

    feats.pipelineStatisticsQuery = true;
    feats.multiDrawIndirect = true;
    feats.drawIndirectFirstInstance = true;
    feats.samplerAnisotropy = true;
    selector.set_required_features(feats);

    vkb::PhysicalDevice physicalDevice = selector
            .set_minimum_version(1, 2)
            .set_surface(mSurface)
            .add_required_extension("VK_KHR_shader_draw_parameters")
            .add_required_extension(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME)
            .select()
            .value();

    Log::info("GPU found");

    //create the final Vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a Vulkan application
    mDevice = vkbDevice.device;
    mChosenGPU = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    //initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = mChosenGPU;
    allocatorInfo.device = mDevice;
    allocatorInfo.instance = mInstance;
    vmaCreateAllocator(&allocatorInfo, &mAllocator);

    vkGetPhysicalDeviceProperties(mChosenGPU, &mGpuProperties);

    Log::info("The gpu has a minimum buffer alignement of " + mGpuProperties.limits.minUniformBufferOffsetAlignment);
}

uint32_t previousPow2(uint32_t v)
{
    uint32_t r = 1;

    while (r * 2 < v)
        r *= 2;

    return r;
}

uint32_t getImageMipLevels(uint32_t width, uint32_t height)
{
    uint32_t result = 1;

    while (width > 1 || height > 1)
    {
        result++;
        width /= 2;
        height /= 2;
    }

    return result;
}

void VulkanEngine::init_swapchain() {
    vkb::SwapchainBuilder swapchainBuilder{ mChosenGPU, mDevice, mSurface };

    vkb::Swapchain vkbSwapchain = swapchainBuilder
            .use_default_format_selection()
                    //use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .set_desired_extent(mWindowExtent.width, mWindowExtent.height)

            .build()
            .value();

    //store swapchain and its related images
    mSwapchain = vkbSwapchain.swapchain;
    mSwapchainImages = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    mSwapchainImageFormat = vkbSwapchain.image_format;

    //render image
    {
        //depth image size will match the window
        VkExtent3D renderImageExtent = {
                mWindowExtent.width,
                mWindowExtent.height,
                1
        };
        mRenderFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkImageCreateInfo ri_info = vkslime::image_create_info(mRenderFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT| VK_IMAGE_USAGE_SAMPLED_BIT, renderImageExtent);

        //for the depth image, we want to allocate it from gpu local memory
        VmaAllocationCreateInfo dimg_allocinfo = {};
        dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        //allocate and create the image
        vmaCreateImage(mAllocator, &ri_info, &dimg_allocinfo, &mRawRenderImage.mImage, &mRawRenderImage.mAllocation, nullptr);

        //build a image-view for the depth image to use for rendering
        VkImageViewCreateInfo dview_info = vkslime::imageview_create_info(mRenderFormat, mRawRenderImage.mImage, VK_IMAGE_ASPECT_COLOR_BIT);

        VK_CHECK_RESULT(vkCreateImageView(mDevice, &dview_info, nullptr, &mRawRenderImage.mDefaultView));
    }


    mMainDeletionQueue.push_function([=]() {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    });

    //depth image size will match the window
    VkExtent3D depthImageExtent = {
            mWindowExtent.width,
            mWindowExtent.height,
            1
    };

    VkExtent3D shadowExtent = {
            mShadowExtent.width,
            mShadowExtent.height,
            1
    };

    //hardcoding the depth format to 32 bit float
    mDepthFormat = VK_FORMAT_D32_SFLOAT;

    //for the depth image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // depth image ------
    {
        //the depth image will be a image with the format we selected and Depth Attachment usage flag
        VkImageCreateInfo dimg_info = vkslime::image_create_info(mDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, depthImageExtent);


        //allocate and create the image
        vmaCreateImage(mAllocator, &dimg_info, &dimg_allocinfo, &mDepthImage.mImage, &mDepthImage.mAllocation, nullptr);


        //build a image-view for the depth image to use for rendering
        VkImageViewCreateInfo dview_info = vkslime::imageview_create_info(mDepthFormat, mDepthImage.mImage, VK_IMAGE_ASPECT_DEPTH_BIT);;

        VK_CHECK_RESULT(vkCreateImageView(mDevice, &dview_info, nullptr, &mDepthImage.mDefaultView));
    }
    //shadow image
    {
        //the depth image will be a image with the format we selected and Depth Attachment usage flag
        VkImageCreateInfo dimg_info = vkslime::image_create_info(mDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, shadowExtent);

        //allocate and create the image
        vmaCreateImage(mAllocator, &dimg_info, &dimg_allocinfo, &mShadowImage.mImage, &mShadowImage.mAllocation, nullptr);

        //build a image-view for the depth image to use for rendering
        VkImageViewCreateInfo dview_info = vkslime::imageview_create_info(mDepthFormat, mShadowImage.mImage, VK_IMAGE_ASPECT_DEPTH_BIT);

        VK_CHECK_RESULT(vkCreateImageView(mDevice, &dview_info, nullptr, &mShadowImage.mDefaultView));
    }


    // Note: previousPow2 makes sure all reductions are at most by 2x2 which makes sure they are conservative
    depthPyramidWidth = previousPow2(mWindowExtent.width);
    depthPyramidHeight = previousPow2(mWindowExtent.height);
    depthPyramidLevels = getImageMipLevels(depthPyramidWidth, depthPyramidHeight);

    VkExtent3D pyramidExtent = {
            static_cast<uint32_t>(depthPyramidWidth),
            static_cast<uint32_t>(depthPyramidHeight),
            1
    };
    //the depth image will be a image with the format we selected and Depth Attachment usage flag
    VkImageCreateInfo pyramidInfo = vkslime::image_create_info(VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, pyramidExtent);

    pyramidInfo.mipLevels = depthPyramidLevels;

    //allocate and create the image
    vmaCreateImage(mAllocator, &pyramidInfo, &dimg_allocinfo, &mDepthPyramid.mImage, &mDepthPyramid.mAllocation, nullptr);

    //build a image-view for the depth image to use for rendering
    VkImageViewCreateInfo priview_info = vkslime::imageview_create_info(VK_FORMAT_R32_SFLOAT, mDepthPyramid.mImage, VK_IMAGE_ASPECT_COLOR_BIT);
    priview_info.subresourceRange.levelCount = depthPyramidLevels;


    VK_CHECK_RESULT(vkCreateImageView(mDevice, &priview_info, nullptr, &mDepthPyramid.mDefaultView));


    for (int32_t i = 0; i < depthPyramidLevels; ++i)
    {
        VkImageViewCreateInfo level_info = vkslime::imageview_create_info(VK_FORMAT_R32_SFLOAT, mDepthPyramid.mImage, VK_IMAGE_ASPECT_COLOR_BIT);
        level_info.subresourceRange.levelCount = 1;
        level_info.subresourceRange.baseMipLevel = i;

        VkImageView pyramid;
        vkCreateImageView(mDevice, &level_info, nullptr, &pyramid);

        mDepthPyramidMips[i] = pyramid;
        assert(mDepthPyramidMips[i]);
    }

    VkSamplerCreateInfo createInfo = {};

    auto reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;

    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.minLod = 0;
    createInfo.maxLod = 16.f;

    VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

    if (reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT)
    {
        createInfoReduction.reductionMode = reductionMode;

        createInfo.pNext = &createInfoReduction;
    }


    VK_CHECK_RESULT(vkCreateSampler(mDevice, &createInfo, 0, &mDepthSampler));

    VkSamplerCreateInfo samplerInfo = vkslime::sampler_create_info(VK_FILTER_LINEAR);
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(mDevice, &samplerInfo, nullptr, &mSmoothSampler);

    VkSamplerCreateInfo shadsamplerInfo = vkslime::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    shadsamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    shadsamplerInfo.compareEnable = true;
    shadsamplerInfo.compareOp = VK_COMPARE_OP_LESS;
    vkCreateSampler(mDevice, &shadsamplerInfo, nullptr, &mShadowSampler);


    //add to deletion queues
    mMainDeletionQueue.push_function([=]() {
        vkDestroyImageView(mDevice, mDepthImage.mDefaultView, nullptr);
        vmaDestroyImage(mAllocator, mDepthImage.mImage, mDepthImage.mAllocation);
    });
}

void VulkanEngine::init_forward_renderpass()
{
    //we define an attachment description for our main color image
    //the attachment is loaded as "clear" when renderpass start
    //the attachment is stored when renderpass ends
    //the attachment layout starts as "undefined", and transitions to "Present" so its possible to display it
    //we dont care about stencil, and dont use multisampling

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = mRenderFormat;//_swachainImageFormat;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment = {};
    // Depth attachment
    depth_attachment.flags = 0;
    depth_attachment.format = mDepthFormat;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    //hook the depth attachment into the subpass
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    //1 dependency, which is from "outside" into the subpass. And we can read or write color
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


    //array of 2 attachments, one for the color, and other for depth
    VkAttachmentDescription attachments[2] = { color_attachment,depth_attachment };

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    //2 attachments from said array
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = &attachments[0];
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    //render_pass_info.dependencyCount = 1;
    //render_pass_info.pDependencies = &dependency;

    VK_CHECK_RESULT(vkCreateRenderPass(mDevice, &render_pass_info, nullptr, &mRenderPass));

    mMainDeletionQueue.push_function([=]() {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    });
}

void VulkanEngine::init_pipelines()
{
    mMaterialSystem = new vkutil::MaterialSystem();
    mMaterialSystem->init(this);
    mMaterialSystem->build_default_templates();

    //fullscreen triangle pipeline for blits
    ShaderEffect* blitEffect = new ShaderEffect();
    blitEffect->add_stage(mShaderCache.get_shader(shader_path("fullscreen.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
    blitEffect->add_stage(mShaderCache.get_shader(shader_path("blit.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);
    blitEffect->reflect_layout(mDevice, nullptr, 0);


    PipelineBuilder pipelineBuilder;

    //input assembly is the configuration for drawing triangle lists, strips, or individual points.
    //we are just going to draw triangle list
    pipelineBuilder._inputAssembly = vkslime::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    //configure the rasterizer to draw filled triangles
    pipelineBuilder._rasterizer = vkslime::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
    pipelineBuilder._rasterizer.cullMode = VK_CULL_MODE_NONE;
    //we dont use multisampling, so just run the default one
    pipelineBuilder._multisampling = vkslime::multisampling_state_create_info();

    //a single blend attachment with no blending and writing to RGBA
    pipelineBuilder._colorBlendAttachment = vkslime::color_blend_attachment_state();


    //default depthtesting
    pipelineBuilder._depthStencil = vkslime::depth_stencil_create_info(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //build blit pipeline
    pipelineBuilder.setShaders(blitEffect);

    //blit pipeline uses hardcoded triangle so no need for vertex input
    pipelineBuilder.clear_vertex_input();

    pipelineBuilder._depthStencil = vkslime::depth_stencil_create_info(false, false, VK_COMPARE_OP_ALWAYS);

    mBlitPipeline = pipelineBuilder.build_pipeline(mDevice, mCopyPass);
    mBlitLayout = blitEffect->builtLayout;

    mMainDeletionQueue.push_function([=]() {
        //vkDestroyPipeline(_device, meshPipeline, nullptr);
        vkDestroyPipeline(mDevice, mBlitPipeline, nullptr);
    });


    //load the compute shaders
    load_compute_shader(shader_path("indirect_cull.comp.spv").c_str(), mCullPipeline, mCullLayout);

    load_compute_shader(shader_path("depthReduce.comp.spv").c_str(), mDepthReducePipeline, mDepthReduceLayout);

    load_compute_shader(shader_path("sparse_upload.comp.spv").c_str(), mSparseUploadPipeline, mSparseUploadLayout);
}

bool VulkanEngine::load_compute_shader(const char* shaderPath, VkPipeline& pipeline, VkPipelineLayout& layout)
{
    ShaderModule computeModule;
    if (!vkslime::load_shader_module(mDevice, shaderPath, &computeModule))

    {
        std::cout << "Error when building compute shader shader module" << std::endl;
        return false;
    }

    ShaderEffect* computeEffect = new ShaderEffect();;
    computeEffect->add_stage(&computeModule, VK_SHADER_STAGE_COMPUTE_BIT);

    computeEffect->reflect_layout(mDevice, nullptr, 0);

    ComputePipelineBuilder computeBuilder;
    computeBuilder._pipelineLayout = computeEffect->builtLayout;
    computeBuilder._shaderStage = vkslime::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, computeModule.module);


    layout = computeEffect->builtLayout;
    pipeline = computeBuilder.build_pipeline(mDevice);

    vkDestroyShaderModule(mDevice, computeModule.module, nullptr);

    mMainDeletionQueue.push_function([=]() {
        vkDestroyPipeline(mDevice, pipeline, nullptr);

        vkDestroyPipelineLayout(mDevice, layout, nullptr);
    });

    return true;
}

void VulkanEngine::load_meshes()
{
    Mesh triMesh{};
    triMesh.bounds.valid = false;
    //make the array 3 vertices long
    triMesh.mVertices.resize(3);

    //vertex positions
    triMesh.mVertices[0].position = { 1.f,1.f, 0.0f };
    triMesh.mVertices[1].position = { -1.f,1.f, 0.0f };
    triMesh.mVertices[2].position = { 0.f,-1.f, 0.0f };

    //vertex colors, all green
    triMesh.mVertices[0].color = { 0.f,1.f, 0.0f }; //pure green
    triMesh.mVertices[1].color = { 0.f,1.f, 0.0f }; //pure green
    triMesh.mVertices[2].color = { 0.f,1.f, 0.0f }; //pure green
    //we dont care about the vertex normals
    upload_mesh(triMesh);
    mMeshes["triangle"] = triMesh;
}

void VulkanEngine::init_copy_renderpass()
{
    //we define an attachment description for our main color image
//the attachment is loaded as "clear" when renderpass start
//the attachment is stored when renderpass ends
//the attachment layout starts as "undefined", and transitions to "Present" so its possible to display it
//we dont care about stencil, and dont use multisampling

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = mSwapchainImageFormat;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    //we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    //1 dependency, which is from "outside" into the subpass. And we can read or write color
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    //2 attachments from said array
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    //render_pass_info.dependencyCount = 1;
    //render_pass_info.pDependencies = &dependency;

    VK_CHECK_RESULT(vkCreateRenderPass(mDevice, &render_pass_info, nullptr, &mCopyPass));

    mMainDeletionQueue.push_function([=]() {
        vkDestroyRenderPass(mDevice, mCopyPass, nullptr);
    });
}

void VulkanEngine::init_shadow_renderpass()
{
    VkAttachmentDescription depth_attachment = {};
    // Depth attachment
    depth_attachment.flags = 0;
    depth_attachment.format = mDepthFormat;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment =0;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    //hook the depth attachment into the subpass
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    //1 dependency, which is from "outside" into the subpass. And we can read or write color
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    //2 attachments from said array
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &depth_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VK_CHECK_RESULT(vkCreateRenderPass(mDevice, &render_pass_info, nullptr, &mShadowPass));

    mMainDeletionQueue.push_function([=]() {
        vkDestroyRenderPass(mDevice, mShadowPass, nullptr);
    });
}

void VulkanEngine::init_commands() {
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkslime::command_pool_create_info(mGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);


    for (auto & frame : mFrames) {


        VK_CHECK_RESULT(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &frame._commandPool));

        //allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkslime::command_buffer_allocate_info(frame._commandPool, 1);

        VK_CHECK_RESULT(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &frame._mainCommandBuffer));

        mMainDeletionQueue.push_function([=]() {
            vkDestroyCommandPool(mDevice, frame._commandPool, nullptr);
        });


    }
    mGraphicsQueueContext = TracyVkContext(mChosenGPU, mDevice, mGraphicsQueue, mFrames[0].mMainCommandBuffer);


    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkslime::command_pool_create_info(mGraphicsQueueFamily);
    //create pool for upload context
    VK_CHECK_RESULT(vkCreateCommandPool(mDevice, &uploadCommandPoolInfo, nullptr, &mUploadContext.mCommandPool));

    mMainDeletionQueue.push_function([=]() {
        vkDestroyCommandPool(mDevice, mUploadContext.mCommandPool, nullptr);
    });
}

void VulkanEngine::cleanup() {
    if (mIsInitialized) {

        //make sure the gpu has stopped doing its things
        for (auto& frame : mFrames)
        {
            vkWaitForFences(mDevice, 1, &frame.renderFence, true, 1000000000);
        }

        mMainDeletionQueue.flush();

        for (auto& frame : mFrames)
        {
            frame.dynamicDescriptorAllocator->cleanup();
        }

        mDescriptorAllocator->cleanup();
        mDescriptorLayoutCache->cleanup();


        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

        vkDestroyDevice(mDevice, nullptr);
        vkDestroyInstance(mInstance, nullptr);

        SDL_DestroyWindow(mWindow);
    }
}

void VulkanEngine::draw() {
    ZoneScopedN("Engine Draw")
    ImGui::Render();

    {
        ZoneScopedN("Fence Wait");
        //wait until the gpu has finished rendering the last frame. Timeout of 1 second
        VK_CHECK_RESULT(vkWaitForFences(mDevice, 1, &get_current_frame().renderFence, true, 1000000000));
        VK_CHECK_RESULT(vkResetFences(mDevice, 1, &get_current_frame().renderFence));

        get_current_frame().dynamicData.reset();

        mRenderScene.build_batches();
        //check the debug data
        void* data;
        vmaMapMemory(mAllocator, get_current_frame().debugOutputBuffer.mAllocation, &data);
        for (int i =1 ; i <   get_current_frame().debugDataNames.size();i++)
        {
            uint32_t begin = get_current_frame().debugDataOffsets[i-1];
            uint32_t end = get_current_frame().debugDataOffsets[i];

            auto name = get_current_frame().debugDataNames[i];
            if (name.compare("Cull Indirect Output") == 0)
            {
                void* buffer = malloc(end - begin);
                memcpy(buffer, (uint8_t*)data + begin, end - begin);

                GPUIndirectObject* objects = (GPUIndirectObject*)buffer;
                int objectCount = (end - begin) / sizeof(GPUIndirectObject);

                std::string filename = fmt::format("{}_CULLDATA_{}.txt", mFrameNumber,i);

                auto out = fmt::output_file(filename);

                for (int o = 0; o < objectCount; o++)
                {
                    out.print("DRAW: {} ------------ \n", o);
                    out.print("	OG Count: {} \n", mRenderScene._forwardPass.batches[o].count);
                    out.print("	Visible Count: {} \n", objects[o].command.instanceCount);
                    out.print("	First: {} \n", objects[o].command.firstInstance);
                    out.print("	Indices: {} \n", objects[o].command.indexCount);
                }

                free(buffer);
            }
        }

        vmaUnmapMemory(mAllocator, get_current_frame().debugOutputBuffer.mAllocation);
        get_current_frame().debugDataNames.clear();
        get_current_frame().debugDataOffsets.clear();

        get_current_frame().debugDataNames.push_back("");
        get_current_frame().debugDataOffsets.push_back(0);
    }
    get_current_frame()._frameDeletionQueue.flush();
    get_current_frame().dynamicDescriptorAllocator->reset_pools();

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK_RESULT(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));
    uint32_t swapchainImageIndex;
    {
        ZoneScopedN("Aquire Image");
        //request image from the swapchain

        VK_CHECK_RESULT(vkAcquireNextImageKHR(mDevice, mSwapchain, 0, get_current_frame()._presentSemaphore, nullptr, &swapchainImageIndex));

    }

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkslime::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearValue clearValue;
    clearValue.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };

    mProfiler->grab_queries(cmd);

    if (!mRenderScene.renderables.empty()){

        postCullBarriers.clear();
        cullReadyBarriers.clear();

        TracyVkZone(_graphicsQueueContext, get_current_frame()._mainCommandBuffer, "All Frame");
        ZoneScopedNC("Render Frame", tracy::Color::White);

        vkutil::VulkanScopeTimer timer(cmd, mProfiler, "All Frame");

        {
            vkutil::VulkanScopeTimer timer2(cmd, mProfiler, "Ready Frame");

            ready_mesh_draw(cmd);

            ready_cull_data(mRenderScene._forwardPass, cmd);
            ready_cull_data(mRenderScene._transparentForwardPass, cmd);
            ready_cull_data(mRenderScene._shadowPass, cmd);

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, cullReadyBarriers.size(), cullReadyBarriers.data(), 0, nullptr);
        }


        CullParams forwardCull;
        forwardCull.projmat = mCamera.get_projection_matrix(true);
        forwardCull.viewmat = mCamera.get_view_matrix();
        forwardCull.frustrumCull = true;
        forwardCull.occlusionCull = true;
        forwardCull.drawDist = forwardCull.drawDist;
        forwardCull.aabb = false;
        {
            execute_compute_cull(cmd, mRenderScene._forwardPass, forwardCull);
            execute_compute_cull(cmd, mRenderScene._transparentForwardPass, forwardCull);
        }

        glm::vec3 extent = mMainLight.shadowExtent * 10.f;
        glm::mat4 projection = glm::orthoLH_ZO(-extent.x, extent.x, -extent.y, extent.y, -extent.z, extent.z);


        CullParams shadowCull;
        shadowCull.projmat = mMainLight.get_projection();
        shadowCull.viewmat = mMainLight.get_view();
        shadowCull.frustrumCull = true;
        shadowCull.occlusionCull = false;
        shadowCull.drawDist = 9999999;
        shadowCull.aabb = true;

        glm::vec3 aabbcenter = mMainLight.lightPosition;
        glm::vec3 aabbextent = mMainLight.shadowExtent * 1.5f;
        shadowCull.aabbmax = aabbcenter + aabbextent;
        shadowCull.aabbmin = aabbcenter - aabbextent;

        {
            vkutil::VulkanScopeTimer timer2(cmd, mProfiler, "Shadow Cull");

            execute_compute_cull(cmd, mRenderScene._shadowPass, shadowCull);
        }

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, postCullBarriers.size(), postCullBarriers.data(), 0, nullptr);

        shadow_pass(cmd);
    }


    forward_pass(clearValue, cmd);

    reduce_depth(cmd);

    copy_render_to_swapchain(swapchainImageIndex, cmd);

    TracyVkCollect(_graphicsQueueContext, get_current_frame()._mainCommandBuffer);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    VkSubmitInfo submit = vkslime::submit_info(&cmd);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &waitStage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &get_current_frame()._presentSemaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &get_current_frame()._renderSemaphore;
    {
        ZoneScopedN("Queue Submit");
        //submit command buffer to the queue and execute it.
        // renderFence will now block until the graphic commands finish execution
        VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &submit, get_current_frame().renderFence));

    }
    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = vkslime::present_info();

    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    {
        ZoneScopedN("Queue Present");
        VK_CHECK_RESULT(vkQueuePresentKHR(mGraphicsQueue, &presentInfo));

    }
    //increase the number of frames drawn
    mFrameNumber++;
}

void VulkanEngine::run() {
    Log::info("Starting Main Loop");

    bool bQuit = false;

    // Using time point and system_clock
    std::chrono::time_point<std::chrono::system_clock> start, end;

    start = std::chrono::system_clock::now();
    end = std::chrono::system_clock::now();
    //main loop
    while (!bQuit)
    {
        ZoneScopedN("Main Loop");
        end = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsed_seconds = end - start;
        stats.frametime = elapsed_seconds.count() * 1000.f;

        start = std::chrono::system_clock::now();
        //Handle events on queue
        SDL_Event e;
        {
            ZoneScopedNC("Event Loop", tracy::Color::White);
            while (SDL_PollEvent(&e) != 0)
            {

                ImGui_ImplSDL2_ProcessEvent(&e);
                mCamera.process_input_event(&e);


                //close the window when user alt-f4s or clicks the X button
                if (e.type == SDL_QUIT)
                {
                    bQuit = true;
                }
                else if (e.type == SDL_KEYDOWN)
                {
                    if (e.key.keysym.sym == SDLK_SPACE)
                    {
                        mSelectedShader += 1;
                        if (mSelectedShader > 1)
                        {
                            mSelectedShader = 0;
                        }
                    }
                }
            }
        }
        {
            ZoneScopedNC("Imgui Logic", tracy::Color::Grey);


            layer.draw(mWindow);

            ImGui::Begin("engine");

            ImGui::Text("Frametimes: %f", stats.frametime);
            ImGui::Text("Objects: %d", stats.objects);
            //ImGui::Text("Drawcalls: %d", stats.drawcalls);
            ImGui::Text("Batches: %d", stats.draws);
            //ImGui::Text("Triangles: %d", stats.triangles);

            ImGui::Separator();

            for (auto& [k, v] : mProfiler->timing)
            {
                ImGui::Text("TIME %s %f ms",k.c_str(), v);
            }
            for (auto& [k, v] : mProfiler->stats)
            {
                ImGui::Text("STAT %s %d", k.c_str(), v);
            }


            ImGui::End();
        }

        {
            ZoneScopedNC("Flag Objects", tracy::Color::Blue);
            //test flagging some objects for changes
            if (!mRenderScene.renderables.empty()) {
                int N_changes = 100;
                for (int i = 0; i < N_changes; i++) {
                    int rng = rand() % mRenderScene.renderables.size();

                    Handle <RenderObject> h{};
                    h.handle = rng;
                    mRenderScene.update_object(h);
                }
            }

            mCamera.bLocked = false;

            mCamera.update_camera(stats.frametime);

            mMainLight.lightPosition = mCamera.position;
        }

        draw();
    }
}

void VulkanEngine::init_framebuffers() {
    const uint32_t swapchain_imagecount = static_cast<uint32_t>(mSwapchainImages.size());
    mFramebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

    //create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
    VkFramebufferCreateInfo fwd_info = vkslime::framebuffer_create_info(mRenderPass, mWindowExtent);
    VkImageView attachments[2];
    attachments[0] = mRawRenderImage.mDefaultView;
    attachments[1] = mDepthImage.mDefaultView;

    fwd_info.pAttachments = attachments;
    fwd_info.attachmentCount = 2;
    VK_CHECK_RESULT(vkCreateFramebuffer(mDevice, &fwd_info, nullptr, &mForwardFramebuffer));

    //create the framebuffer for shadow pass
    VkFramebufferCreateInfo sh_info = vkslime::framebuffer_create_info(mShadowPass, mShadowExtent);
    sh_info.pAttachments = &mShadowImage.mDefaultView;
    sh_info.attachmentCount = 1;
    VK_CHECK_RESULT(vkCreateFramebuffer(mDevice, &sh_info, nullptr, &mShadowFramebuffer));

    for (uint32_t i = 0; i < swapchain_imagecount; i++) {

        //create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
        VkFramebufferCreateInfo fb_info = vkslime::framebuffer_create_info(mCopyPass, mWindowExtent);
        fb_info.pAttachments = &mSwapchainImageViews[i];
        fb_info.attachmentCount = 1;
        VK_CHECK_RESULT(vkCreateFramebuffer(mDevice, &fb_info, nullptr, &mFramebuffers[i]));

        mMainDeletionQueue.push_function([=]() {
            vkDestroyFramebuffer(mDevice, mFramebuffers[i], nullptr);
            vkDestroyImageView(mDevice, mSwapchainImageViews[i], nullptr);
        });
    }
}

void VulkanEngine::forward_pass(VkClearValue clearValue, VkCommandBuffer cmd)
{
    vkutil::VulkanScopeTimer timer(cmd, mProfiler, "Forward Pass");
    vkutil::VulkanPipelineStatRecorder timer2(cmd, mProfiler, "Forward Primitives");
    //clear depth at 0
    VkClearValue depthClear;
    depthClear.depthStencil.depth = 0.f;

    //start the main renderpass.
    //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    VkRenderPassBeginInfo rpInfo = vkslime::renderpass_begin_info(mRenderPass, mWindowExtent, mForwardFramebuffer/*_framebuffers[swapchainImageIndex]*/);

    //connect clear values
    rpInfo.clearValueCount = 2;

    VkClearValue clearValues[] = { clearValue, depthClear };

    rpInfo.pClearValues = &clearValues[0];
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)mWindowExtent.width;
    viewport.height = (float)mWindowExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = mWindowExtent;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdSetDepthBias(cmd, 0, 0, 0);


    //stats.drawcalls = 0;
    //stats.draws = 0;
    //stats.objects = 0;
    //stats.triangles = 0;

    {
        TracyVkZone(mGraphicsQueueContext, get_current_frame()._mainCommandBuffer, "Forward Pass");
        draw_objects_forward(cmd, mRenderScene._forwardPass);
        draw_objects_forward(cmd, mRenderScene._transparentForwardPass);
    }


    {
        TracyVkZone(mGraphicsQueueContext, get_current_frame().mMainCommandBuffer, "Imgui Draw");
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }

    //finalize the render pass
    vkCmdEndRenderPass(cmd);
}

void VulkanEngine::shadow_pass(VkCommandBuffer cmd)
{
    vkutil::VulkanScopeTimer timer(cmd, mProfiler, "Shadow Pass");
    vkutil::VulkanPipelineStatRecorder timer2(cmd, mProfiler, "Shadow Primitives");

    //clear depth at 1
    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.f;
    VkRenderPassBeginInfo rpInfo = vkslime::renderpass_begin_info(mShadowPass, mShadowExtent, mShadowFramebuffer);

    //connect clear values
    rpInfo.clearValueCount = 1;

    VkClearValue clearValues[] = { depthClear };

    rpInfo.pClearValues = &clearValues[0];
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)mShadowExtent.width;
    viewport.height = (float)mShadowExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = mShadowExtent;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);



    stats.drawcalls = 0;
    stats.draws = 0;
    stats.objects = 0;
    stats.triangles = 0;

    if(!mRenderScene._shadowPass.batches.empty())
    {
        TracyVkZone(mGraphicsQueueContext, get_current_frame().mMainCommandBuffer, "Shadow  Pass");
        draw_objects_shadow(cmd, mRenderScene._shadowPass);
    }

    //finalize the render pass
    vkCmdEndRenderPass(cmd);
}

void VulkanEngine::init_sync_structures() {
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to syncronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkslime::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    VkSemaphoreCreateInfo semaphoreCreateInfo = vkslime::semaphore_create_info();

    for (auto & frame : mFrames) {

        VK_CHECK_RESULT(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &frame.renderFence));

        //enqueue the destruction of the fence
        mMainDeletionQueue.push_function([=]() {
            vkDestroyFence(mDevice, frame.renderFence, nullptr);
        });


        VK_CHECK_RESULT(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &frame._presentSemaphore));
        VK_CHECK_RESULT(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));

        //enqueue the destruction of semaphores
        mMainDeletionQueue.push_function([=]() {
            vkDestroySemaphore(mDevice, frame._presentSemaphore, nullptr);
            vkDestroySemaphore(mDevice, frame._renderSemaphore, nullptr);
        });
    }


    VkFenceCreateInfo uploadFenceCreateInfo = vkslime::fence_create_info();

    VK_CHECK_RESULT(vkCreateFence(mDevice, &uploadFenceCreateInfo, nullptr, &mUploadContext.mUploadFence));
    mMainDeletionQueue.push_function([=]() {
        vkDestroyFence(mDevice, mUploadContext.mUploadFence, nullptr);
    });
}

void VulkanEngine::copy_render_to_swapchain(uint32_t swapchainImageIndex, VkCommandBuffer cmd)
{
    //start the main renderpass.
    //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    VkRenderPassBeginInfo copyRP = vkslime::renderpass_begin_info(mCopyPass, mWindowExtent, mFramebuffers[swapchainImageIndex]);


    vkCmdBeginRenderPass(cmd, &copyRP, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)mWindowExtent.width;
    viewport.height = (float)mWindowExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = mWindowExtent;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdSetDepthBias(cmd, 0, 0, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mBlitPipeline);

    VkDescriptorImageInfo sourceImage;
    sourceImage.sampler = mSmoothSampler;

    sourceImage.imageView = mRawRenderImage.mDefaultView;
    sourceImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorSet blitSet;
    vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
            .bind_image(0, &sourceImage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(blitSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mBlitLayout, 0, 1, &blitSet, 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0);


    vkCmdEndRenderPass(cmd);
}

void VulkanEngine::process_input_event(SDL_Event* ev)
{
    if (ev->type == SDL_KEYDOWN)
    {
        switch (ev->key.keysym.sym)
        {

        }
    }
    else if (ev->type == SDL_KEYUP)
    {
        switch (ev->key.keysym.sym)
        {
            case SDLK_UP:
            case SDLK_w:
                mCamera.inputAxis.x -= 1.f;
                break;
            case SDLK_DOWN:
            case SDLK_s:
                mCamera.inputAxis.x += 1.f;
                break;
            case SDLK_LEFT:
            case SDLK_a:
                mCamera.inputAxis.y += 1.f;
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                mCamera.inputAxis.y -= 1.f;
                break;
        }
    }

    mCamera.inputAxis = glm::clamp(mCamera.inputAxis, { -1.0,-1.0,-1.0 }, { 1.0,1.0,1.0 });
}


void VulkanEngine::upload_mesh(Mesh& mesh)
{
    ZoneScopedNC("Upload Mesh", tracy::Color::Orange);

    const size_t vertex_buffer_size = mesh.mVertices.size() * sizeof(Vertex);
    const size_t index_buffer_size = mesh.mIndices.size() * sizeof(uint32_t);
    const size_t bufferSize = vertex_buffer_size + index_buffer_size;
    //allocate vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    vertexBufferInfo.size = vertex_buffer_size;
    //this buffer is going to be used as a Vertex Buffer
    vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    //allocate vertex buffer
    VkBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    indexBufferInfo.size = index_buffer_size;
    //this buffer is going to be used as a Vertex Buffer
    indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    //let the VMA library know that this data should be writeable by CPU, but also readable by GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    AllocatedBufferUntyped stagingBuffer;

    //allocate the buffer
    VK_CHECK_RESULT(vmaCreateBuffer(mAllocator, &vertexBufferInfo, &vmaallocInfo,
                             &mesh.mVertexBuffer.mBuffer,
                             &mesh.mVertexBuffer.mAllocation,
                             nullptr));
    //copy vertex data
    char* data;
    vmaMapMemory(mAllocator, mesh.mVertexBuffer.mAllocation, (void**)&data);

    memcpy(data, mesh.mVertices.data(), vertex_buffer_size);

    vmaUnmapMemory(mAllocator, mesh.mVertexBuffer.mAllocation);

    if (index_buffer_size != 0)
    {
        //allocate the buffer
        VK_CHECK_RESULT(vmaCreateBuffer(mAllocator, &indexBufferInfo, &vmaallocInfo,
                                 &mesh.mIndexBuffer.mBuffer,
                                 &mesh.mIndexBuffer.mAllocation,
                                 nullptr));
        vmaMapMemory(mAllocator, mesh.mIndexBuffer.mAllocation, (void**)&data);

        memcpy(data, mesh.mIndices.data(), index_buffer_size);

        vmaUnmapMemory(mAllocator, mesh.mIndexBuffer.mAllocation);
    }
}

Mesh *VulkanEngine::get_mesh(const std::string &name) {
    //Search for the object, and return nullptr if not found
    auto it = mMeshes.find(name);
    if (it == mMeshes.end()) {
        Log::error("FAILED TO GET MESH: " + name);
        return nullptr;
    } else
        return &(*it).second;
}

void VulkanEngine::init_scene() {
    VkSamplerCreateInfo samplerInfo = vkslime::sampler_create_info(VK_FILTER_NEAREST);

    VkSampler blockySampler;
    vkCreateSampler(mDevice, &samplerInfo, nullptr, &blockySampler);

    samplerInfo = vkslime::sampler_create_info(VK_FILTER_LINEAR);

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    //info.anisotropyEnable = true;
    samplerInfo.mipLodBias = 2;
    samplerInfo.maxLod = 30.f;
    samplerInfo.minLod = 3;
    VkSampler smoothSampler;

    vkCreateSampler(mDevice, &samplerInfo, nullptr, &smoothSampler);


  {
      vkutil::MaterialData texturedInfo;
      texturedInfo.baseTemplate = "texturedPBR_opaque";
      texturedInfo.parameters = nullptr;

      vkutil::SampledTexture whiteTex{};
      whiteTex.sampler = smoothSampler;
      whiteTex.view = mLoadedTextures["white"].imageView;

      texturedInfo.textures.push_back(whiteTex);

      vkutil::Material* newmat = mMaterialSystem->build_material("textured", texturedInfo);
  }
  {
      vkutil::MaterialData matinfo;
      matinfo.baseTemplate = "texturedPBR_opaque";
      matinfo.parameters = nullptr;

      vkutil::SampledTexture whiteTex{};
      whiteTex.sampler = smoothSampler;
      whiteTex.view = mLoadedTextures["white"].imageView;

      matinfo.textures.push_back(whiteTex);

      vkutil::Material* newmat = mMaterialSystem->build_material("default", matinfo);

  }

   //for (int x = -20; x <= 20; x++) {
   //	for (int y = -20; y <= 20; y++) {

   //		MeshObject tri;
   //		tri.mesh = get_mesh("triangle");
   //		tri.material = mMaterialSystem->get_material("default");
   //		glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
   //		glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
   //		tri.transformMatrix = translation * scale;

   //		refresh_renderbounds(&tri);

   //        //sort key from location
   //        int32_t lx = int(tri.bounds.origin.x / 10.f);
   //        int32_t ly = int(tri.bounds.origin.y / 10.f);

   //        uint32_t key =  uint32_t(std::hash<int32_t>()(lx) ^ std::hash<int32_t>()(ly^1337));

   //        tri.customSortKey = key;// rng;// key;

   //		mRenderScene.register_object(&tri);
   //	}
   //}
}

bool VulkanEngine::load_prefab(const char* path, glm::mat4 root)
{
    int rng = rand();

    ZoneScopedNC("Load Prefab", tracy::Color::Red)

    auto pf = mPrefabCache.find(path);
    if (pf == mPrefabCache.end())
    {
        assets::AssetFile file;
        bool loaded = assets::load_binaryfile(path, file);

        if (!loaded) {
            Log::error("Error When loading prefab file at path " + std::string(path));
            return false;
        }
        else {
            Log::info("Prefab loaded to cache " + std::string(path));
        }

        mPrefabCache[path] = new assets::PrefabInfo;

        *mPrefabCache[path] = assets::read_prefab_info(&file);
    }

    assets::PrefabInfo* prefab = mPrefabCache[path];

    VkSamplerCreateInfo samplerInfo = vkslime::sampler_create_info(VK_FILTER_LINEAR);
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;


    VkSampler smoothSampler;
    vkCreateSampler(mDevice, &samplerInfo, nullptr, &smoothSampler);


    std::unordered_map<uint64_t, glm::mat4> node_worldmats;

    std::vector<std::pair<uint64_t, glm::mat4>> pending_nodes;
    for (auto& [k, v] : prefab->node_matrices)
    {

        glm::mat4 nodematrix{ 1.f };

        auto nm = prefab->matrices[v];
        memcpy(&nodematrix, &nm, sizeof(glm::mat4));

        //check if it has parents
        auto matrixIT = prefab->node_parents.find(k);
        if (matrixIT == prefab->node_parents.end()) {
            //add to worldmats
            node_worldmats[k] = root* nodematrix;
        }
        else {
            //enqueue
            pending_nodes.push_back({ k,nodematrix });
        }
    }

    //process pending nodes list until it empties
    while (pending_nodes.size() > 0)
    {
        for (int i = 0; i < pending_nodes.size(); i++)
        {
            uint64_t node = pending_nodes[i].first;
            uint64_t parent = prefab->node_parents[node];

            //try to find parent in cache
            auto matrixIT = node_worldmats.find(parent);
            if (matrixIT != node_worldmats.end()) {

                //transform with the parent
                glm::mat4 nodematrix = (matrixIT)->second * pending_nodes[i].second;

                node_worldmats[node] = nodematrix;

                //remove from queue, pop last
                pending_nodes[i] = pending_nodes.back();
                pending_nodes.pop_back();
                i--;
            }
        }

    }

    std::vector<MeshObject> prefab_renderables;
    prefab_renderables.reserve(prefab->node_meshes.size());

    for (auto& [k, v] : prefab->node_meshes)
    {

        //load mesh

        if (v.mesh_path.find("Sky") != std::string::npos) {
            continue;
        }

        if (!get_mesh(v.mesh_path.c_str()))
        {
            Mesh mesh{};
            mesh.load_from_meshasset(asset_path(v.mesh_path).c_str());

            upload_mesh(mesh);

            mMeshes[v.mesh_path.c_str()] = mesh;
        }


        auto materialName = v.material_path.c_str();
        //load material

        vkutil::Material* objectMaterial = mMaterialSystem->get_material(materialName);
        if (!objectMaterial)
        {
            assets::AssetFile materialFile;
            bool loaded = assets::load_binaryfile(asset_path(materialName).c_str(), materialFile);

            if (loaded)
            {
                assets::MaterialInfo material = assets::read_material_info(&materialFile);

                auto texture = material.textures["baseColor"];
                if (texture.size() <= 3)
                {
                    texture = "Sponza/white.tx";
                }

                loaded = load_image_to_cache(texture.c_str(), asset_path(texture).c_str());

                if (loaded)
                {
                    vkutil::SampledTexture tex;
                    tex.view = mLoadedTextures[texture].imageView;
                    tex.sampler = smoothSampler;

                    vkutil::MaterialData info;
                    info.parameters = nullptr;

                    if (material.transparency == assets::TransparencyMode::Transparent)
                    {
                        info.baseTemplate = "texturedPBR_transparent";
                    }
                    else {
                        info.baseTemplate = "texturedPBR_opaque";
                    }

                    info.textures.push_back(tex);

                    objectMaterial = mMaterialSystem->build_material(materialName, info);

                    if (!objectMaterial)
                    {
                        Log::error("Error When building material " + v.material_path);
                    }
                }
                else
                {
                    Log::error("Error When loading image at " + v.material_path);
                }
            }
            else
            {
                Log::error("Error When loading material at path " + v.material_path);
            }
        }

        MeshObject loadmesh;
        //transparent objects will be invisible

        loadmesh.bDrawForwardPass = true;
        loadmesh.bDrawShadowPass = true;


        glm::mat4 nodematrix{ 1.f };

        auto matrixIT = node_worldmats.find(k);
        if (matrixIT != node_worldmats.end()) {
            auto nm = (*matrixIT).second;
            memcpy(&nodematrix, &nm, sizeof(glm::mat4));
        }

        loadmesh.mesh = get_mesh(v.mesh_path.c_str());
        loadmesh.transformMatrix = nodematrix;
        loadmesh.material = objectMaterial;

        refresh_renderbounds(&loadmesh);

        //sort key from location
        int32_t lx = int(loadmesh.bounds.origin.x / 10.f);
        int32_t ly = int(loadmesh.bounds.origin.y / 10.f);

        uint32_t key =  uint32_t(std::hash<int32_t>()(lx) ^ std::hash<int32_t>()(ly^1337));

        loadmesh.customSortKey = 0;// rng;// key;


        prefab_renderables.push_back(loadmesh);
        //_renderables.push_back(loadmesh);
    }

    mRenderScene.register_object_batch(prefab_renderables.data(), static_cast<uint32_t>(prefab_renderables.size()));

    return true;
}


std::string VulkanEngine::asset_path(std::string_view path)
{
    return "../assets/" + std::string(path);
}

std::string VulkanEngine::shader_path(std::string_view path)
{
    return "../shaders/" + std::string(path);
}

void VulkanEngine::refresh_renderbounds(MeshObject* object)
{
    //dont try to update invalid bounds
    if (!object->mesh->bounds.valid) return;

    RenderBounds originalBounds = object->mesh->bounds;

    //convert bounds to 8 vertices, and transform those
    std::array<glm::vec3, 8> boundsVerts;

    for (int i = 0; i < 8; i++) {
        boundsVerts[i] = originalBounds.origin;
    }

    boundsVerts[0] += originalBounds.extents * glm::vec3(1, 1, 1);
    boundsVerts[1] += originalBounds.extents * glm::vec3(1, 1, -1);
    boundsVerts[2] += originalBounds.extents * glm::vec3(1, -1, 1);
    boundsVerts[3] += originalBounds.extents * glm::vec3(1, -1, -1);
    boundsVerts[4] += originalBounds.extents * glm::vec3(-1, 1, 1);
    boundsVerts[5] += originalBounds.extents * glm::vec3(-1, 1, -1);
    boundsVerts[6] += originalBounds.extents * glm::vec3(-1, -1, 1);
    boundsVerts[7] += originalBounds.extents * glm::vec3(-1, -1, -1);

    //recalc max/min
    glm::vec3 min{ std::numeric_limits<float>().max() };
    glm::vec3 max{ -std::numeric_limits<float>().max() };

    glm::mat4 m = object->transformMatrix;

    //transform every vertex, accumulating max/min
    for (int i = 0; i < 8; i++) {
        boundsVerts[i] = m * glm::vec4(boundsVerts[i],1.f);

        min = glm::min(boundsVerts[i], min);
        max = glm::max(boundsVerts[i], max);
    }

    glm::vec3 extents = (max - min) / 2.f;
    glm::vec3 origin = min + extents;

    float max_scale = 0;
    max_scale = std::max( glm::length(glm::vec3(m[0][0], m[0][1], m[0][2])),max_scale);
    max_scale = std::max( glm::length(glm::vec3(m[1][0], m[1][1], m[1][2])),max_scale);
    max_scale = std::max( glm::length(glm::vec3(m[2][0], m[2][1], m[2][2])),max_scale);

    float radius = max_scale * originalBounds.radius;


    object->bounds.extents = extents;
    object->bounds.origin = origin;
    object->bounds.radius = radius;
    object->bounds.valid = true;
}

void VulkanEngine::unmap_buffer(AllocatedBufferUntyped& buffer)
{
    vmaUnmapMemory(mAllocator, buffer.mAllocation);
}

void VulkanEngine::ready_cull_data(RenderScene::MeshPass& pass, VkCommandBuffer cmd)
{
    //copy from the cleared indirect buffer into the one we will use on rendering. This one happens every frame
    VkBufferCopy indirectCopy;
    indirectCopy.dstOffset = 0;
    indirectCopy.size = pass.batches.size() * sizeof(GPUIndirectObject);
    indirectCopy.srcOffset = 0;

    vkCmdCopyBuffer(cmd, pass.clearIndirectBuffer.mBuffer, pass.drawIndirectBuffer.mBuffer, 1, &indirectCopy);

    {
        VkBufferMemoryBarrier barrier = vkslime::buffer_barrier(pass.drawIndirectBuffer.mBuffer, mGraphicsQueueFamily);
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        cullReadyBarriers.push_back(barrier);
        //vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }
}

FrameData &VulkanEngine::get_current_frame() {
    return mFrames[mFrameNumber % FRAME_OVERLAP];
}

FrameData& VulkanEngine::get_last_frame()
{
    return mFrames[(mFrameNumber - 1) % 2];
}

AllocatedBufferUntyped
VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags required_flags)
{
    //allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;


    //let the VMA library know that this data should be writeable by CPU, but also readable by GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.requiredFlags = required_flags;
    AllocatedBufferUntyped newBuffer;

    //allocate the buffer
    VK_CHECK_RESULT(vmaCreateBuffer(mAllocator, &bufferInfo, &vmaallocInfo,
                             &newBuffer.mBuffer,
                             &newBuffer.mAllocation,
                             nullptr));
    newBuffer.mSize = allocSize;
    return newBuffer;
}

void VulkanEngine::reallocate_buffer(AllocatedBufferUntyped& buffer, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags required_flags /*= 0*/)
{
    AllocatedBufferUntyped newBuffer = create_buffer(allocSize, usage, memoryUsage, required_flags);

    get_current_frame()._frameDeletionQueue.push_function([=]() {

        vmaDestroyBuffer(mAllocator, buffer.mBuffer, buffer.mAllocation);
    });

    buffer = newBuffer;
}


void VulkanEngine::init_descriptors() {
    mDescriptorAllocator = new vkutil::DescriptorAllocator{};
    mDescriptorAllocator->init(mDevice);

    mDescriptorLayoutCache = new vkutil::DescriptorLayoutCache{};
    mDescriptorLayoutCache->init(mDevice);


    VkDescriptorSetLayoutBinding textureBind = vkslime::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    VkDescriptorSetLayoutCreateInfo set3info = {};
    set3info.bindingCount = 1;
    set3info.flags = 0;
    set3info.pNext = nullptr;
    set3info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set3info.pBindings = &textureBind;

    mSingleTextureSetLayout = mDescriptorLayoutCache->create_descriptor_layout(&set3info);


    const size_t sceneParamBufferSize = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData));


    for (auto & currentFrame : mFrames)
    {
        currentFrame.dynamicDescriptorAllocator = new vkutil::DescriptorAllocator{};
        currentFrame.dynamicDescriptorAllocator->init(mDevice);

        //1 megabyte of dynamic data buffer
        auto dynamicDataBuffer = create_buffer(1000000, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        currentFrame.dynamicData.init(mAllocator, dynamicDataBuffer, mGpuProperties.limits.minUniformBufferOffsetAlignment);

        //20 megabyte of debug output
        currentFrame.debugOutputBuffer = create_buffer(200000000, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
    }
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = mGpuProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function) {

    ZoneScopedNC("Inmediate Submit", tracy::Color::White)

    VkCommandBuffer cmd;

    //allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkslime::command_buffer_allocate_info(mUploadContext.mCommandPool, 1);

    VK_CHECK_RESULT(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &cmd));

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkslime::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);


    VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = vkslime::submit_info(&cmd);


    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &submit, mUploadContext.mUploadFence));

    vkWaitForFences(mDevice, 1, &mUploadContext.mUploadFence, true, 9999999999);
    vkResetFences(mDevice, 1, &mUploadContext.mUploadFence);

    vkResetCommandPool(mDevice, mUploadContext.mCommandPool, 0);
}

void VulkanEngine::load_images() {
    load_image_to_cache("white", asset_path("Sponza/6772804448157695701.jpg").c_str());
}

void VulkanEngine::init_imgui() {
//1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] =
            {
                    {VK_DESCRIPTOR_TYPE_SAMPLER,                1000},
                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000},
                    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000},
                    {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000},
                    {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000},
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000},
                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000},
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                    {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000}
            };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK_RESULT(vkCreateDescriptorPool(mDevice, &pool_info, nullptr, &imguiPool));


    // 2: initialize imgui library

    //this initializes the core structures of imgui
    ImGui::CreateContext();

    //ImGuiIO &io = ImGui::GetIO();
    //float fontSize = 18.0f;
    //io.Fonts->AddFontFromFileTTF("C:/Users/alexm/CLionProjects/VulkanSlime/assets/Fonts/opensans/OpenSans-Bold.ttf",
    //                             fontSize);
    //io.FontDefault = io.Fonts->AddFontFromFileTTF(
    //        "C:/Users/alexm/CLionProjects/VulkanSlime/assets/Fonts/opensans/OpenSans-Regular.ttf", fontSize);

    //this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(mWindow);

    //this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = mInstance;
    init_info.PhysicalDevice = mChosenGPU;
    init_info.Device = mDevice;
    init_info.Queue = mGraphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, mRenderPass);

    //execute a gpu command to upload imgui font textures
    immediate_submit([&](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });

    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    //add the destroy the imgui created structures
    mMainDeletionQueue.push_function([this, imguiPool]() {
        vkDestroyDescriptorPool(mDevice, imguiPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
    });
}

bool VulkanEngine::load_image_to_cache(const char *name, const char *path) {

    ZoneScopedNC("Load Texture", tracy::Color::Yellow)
    Texture newtex{};

    if (mLoadedTextures.find(name) != mLoadedTextures.end()) return true;

    bool result = vkutil::load_image_from_file(*this, path, newtex.image); // need to figure this out load_image_from_asset

    if (!result) {
        Log::error("Error When texture " + std::string(name) + " at path " + path);
        return false;
    } else {
        Log::trace("Loaded texture " + std::string(name) + " at path " + path);
    }
    newtex.imageView = newtex.image.mDefaultView;
    //VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_UNORM, newtex.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
    //imageinfo.subresourceRange.levelCount = newtex.image.mipLevels;
    //vkCreateImageView(_device, &imageinfo, nullptr, &newtex.imageView);

    mLoadedTextures[name] = newtex;
    return true;
}

glm::mat4 DirectionalLight::get_projection()
{
    glm::mat4 projection = glm::orthoLH_ZO(-shadowExtent.x, shadowExtent.x, -shadowExtent.y, shadowExtent.y, -shadowExtent.z, shadowExtent.z);
    return projection;
}

glm::mat4 DirectionalLight::get_view()
{
    glm::vec3 camPos = lightPosition;

    glm::vec3 camFwd = lightDirection;

    glm::mat4 view = glm::lookAt(camPos, camPos + camFwd, glm::vec3(1, 0, 0));
    return view;
}

glm::vec4 normalizePlane(glm::vec4 p)
{
    return p / glm::length(glm::vec3(p));
}

void VulkanEngine::execute_compute_cull(VkCommandBuffer cmd, RenderScene::MeshPass& pass,CullParams& params )
{
    if (pass.batches.size() == 0) return;
    TracyVkZone(_graphicsQueueContext, cmd, "Cull Dispatch");
    VkDescriptorBufferInfo objectBufferInfo = mRenderScene.objectDataBuffer.get_info();

    VkDescriptorBufferInfo dynamicInfo = get_current_frame().dynamicData.source.get_info();
    dynamicInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo instanceInfo = pass.passObjectsBuffer.get_info();

    VkDescriptorBufferInfo finalInfo = pass.compactedInstanceBuffer.get_info();

    VkDescriptorBufferInfo indirectInfo = pass.drawIndirectBuffer.get_info();

    VkDescriptorImageInfo depthPyramid;
    depthPyramid.sampler = mDepthSampler;
    depthPyramid.imageView = mDepthPyramid.mDefaultView;
    depthPyramid.imageLayout = VK_IMAGE_LAYOUT_GENERAL;


    VkDescriptorSet COMPObjectDataSet;
    vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
            .bind_buffer(0, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .bind_buffer(1, &indirectInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .bind_buffer(2, &instanceInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .bind_buffer(3, &finalInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .bind_image(4, &depthPyramid, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .bind_buffer(5, &dynamicInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(COMPObjectDataSet);


    glm::mat4 projection = params.projmat;
    glm::mat4 projectionT = transpose(projection);

    glm::vec4 frustumX = normalizePlane(projectionT[3] + projectionT[0]); // x + w < 0
    glm::vec4 frustumY = normalizePlane(projectionT[3] + projectionT[1]); // y + w < 0

    DrawCullData cullData = {};
    cullData.P00 = projection[0][0];
    cullData.P11 = projection[1][1];
    cullData.znear = 0.1f;
    cullData.zfar = params.drawDist;
    cullData.frustum[0] = frustumX.x;
    cullData.frustum[1] = frustumX.z;
    cullData.frustum[2] = frustumY.y;
    cullData.frustum[3] = frustumY.z;
    cullData.drawCount = static_cast<uint32_t>(pass.flat_batches.size());
    cullData.cullingEnabled = params.frustrumCull;
    cullData.lodEnabled = false;
    cullData.occlusionEnabled = params.occlusionCull;
    cullData.lodBase = 10.f;
    cullData.lodStep = 1.5f;
    cullData.pyramidWidth = static_cast<float>(depthPyramidWidth);
    cullData.pyramidHeight = static_cast<float>(depthPyramidHeight);
    cullData.viewMat = params.viewmat;//get_view_matrix();

    cullData.AABBcheck = params.aabb;
    cullData.aabbmin_x = params.aabbmin.x;
    cullData.aabbmin_y = params.aabbmin.y;
    cullData.aabbmin_z = params.aabbmin.z;

    cullData.aabbmax_x = params.aabbmax.x;
    cullData.aabbmax_y = params.aabbmax.y;
    cullData.aabbmax_z = params.aabbmax.z;

    if (params.drawDist > 10000)
    {
        cullData.distanceCheck = false;
    }
    else
    {
        cullData.distanceCheck = true;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mCullPipeline);

    vkCmdPushConstants(cmd, mCullLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DrawCullData), &cullData);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mCullLayout, 0, 1, &COMPObjectDataSet, 0, nullptr);

    vkCmdDispatch(cmd, static_cast<uint32_t>((pass.flat_batches.size() / 256)+1), 1, 1);


    //barrier the 2 buffers we just wrote for culling, the indirect draw one, and the instances one, so that they can be read well when rendering the pass
    {
        VkBufferMemoryBarrier barrier = vkslime::buffer_barrier(pass.compactedInstanceBuffer.mBuffer, mGraphicsQueueFamily);
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        VkBufferMemoryBarrier barrier2 = vkslime::buffer_barrier(pass.drawIndirectBuffer.mBuffer, mGraphicsQueueFamily);
        barrier2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        VkBufferMemoryBarrier barriers[] = { barrier,barrier2 };

        postCullBarriers.push_back(barrier);
        postCullBarriers.push_back(barrier2);

    }
}
#include <future>
void VulkanEngine::ready_mesh_draw(VkCommandBuffer cmd)
{
    TracyVkZone(_graphicsQueueContext, get_current_frame()._mainCommandBuffer, "Data Refresh");
    ZoneScopedNC("Draw Upload", tracy::Color::Blue);

    //upload object data to gpu

    if (mRenderScene.dirtyObjects.size() > 0)
    {
        ZoneScopedNC("Refresh Object Buffer", tracy::Color::Red);

        size_t copySize = mRenderScene.renderables.size() * sizeof(GPUObjectData);
        if (mRenderScene.objectDataBuffer.mSize < copySize)
        {
            reallocate_buffer(mRenderScene.objectDataBuffer, copySize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        }

        //if 80% of the objects are dirty, then just reupload the whole thing
        if (mRenderScene.dirtyObjects.size() >= mRenderScene.renderables.size() * 0.8)
        {
            AllocatedBuffer<GPUObjectData> newBuffer = create_buffer(copySize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            GPUObjectData* objectSSBO = map_buffer(newBuffer);
            mRenderScene.fill_objectData(objectSSBO);
            unmap_buffer(newBuffer);

            get_current_frame()._frameDeletionQueue.push_function([=]() {

                vmaDestroyBuffer(mAllocator, newBuffer.mBuffer, newBuffer.mAllocation);
            });

            //copy from the uploaded cpu side instance buffer to the gpu one
            VkBufferCopy indirectCopy;
            indirectCopy.dstOffset = 0;
            indirectCopy.size = mRenderScene.renderables.size() * sizeof(GPUObjectData);
            indirectCopy.srcOffset = 0;
            vkCmdCopyBuffer(cmd, newBuffer.mBuffer, mRenderScene.objectDataBuffer.mBuffer, 1, &indirectCopy);
        }
        else {
            //update only the changed elements

            std::vector<VkBufferCopy> copies;
            copies.reserve(mRenderScene.dirtyObjects.size());

            uint64_t buffersize = sizeof(GPUObjectData) * mRenderScene.dirtyObjects.size();
            uint64_t vec4size = sizeof(glm::vec4);
            uint64_t intsize = sizeof(uint32_t);
            uint64_t wordsize = sizeof(GPUObjectData) / sizeof(uint32_t);
            uint64_t uploadSize = mRenderScene.dirtyObjects.size() * wordsize * intsize;
            AllocatedBuffer<GPUObjectData> newBuffer = create_buffer(buffersize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            AllocatedBuffer<uint32_t> targetBuffer = create_buffer(uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            get_current_frame()._frameDeletionQueue.push_function([=]() {

                vmaDestroyBuffer(mAllocator, newBuffer.mBuffer, newBuffer.mAllocation);
                vmaDestroyBuffer(mAllocator, targetBuffer.mBuffer, targetBuffer.mAllocation);
            });

            uint32_t* targetData = map_buffer(targetBuffer);
            GPUObjectData* objectSSBO = map_buffer(newBuffer);
            uint32_t launchcount = static_cast<uint32_t>(mRenderScene.dirtyObjects.size() * wordsize);
            {
                ZoneScopedNC("Write dirty objects", tracy::Color::Red);
                uint32_t sidx = 0;
                for (int i = 0; i < mRenderScene.dirtyObjects.size(); i++)
                {
                    mRenderScene.write_object(objectSSBO + i, mRenderScene.dirtyObjects[i]);


                    uint32_t dstOffset = static_cast<uint32_t>(wordsize * mRenderScene.dirtyObjects[i].handle);

                    for (int b = 0; b < wordsize; b++ )
                    {
                        uint32_t tidx = dstOffset + b;
                        targetData[sidx] = tidx;
                        sidx++;
                    }
                }
                launchcount = sidx;
            }
            unmap_buffer(newBuffer);
            unmap_buffer(targetBuffer);

            VkDescriptorBufferInfo indexData = targetBuffer.get_info();

            VkDescriptorBufferInfo sourceData = newBuffer.get_info();

            VkDescriptorBufferInfo targetInfo = mRenderScene.objectDataBuffer.get_info();

            VkDescriptorSet COMPObjectDataSet;
            vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
                    .bind_buffer(0, &indexData, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                    .bind_buffer(1, &sourceData, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                    .bind_buffer(2, &targetInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                    .build(COMPObjectDataSet);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mSparseUploadPipeline);


            vkCmdPushConstants(cmd, mSparseUploadLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &launchcount);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mSparseUploadLayout, 0, 1, &COMPObjectDataSet, 0, nullptr);

            vkCmdDispatch(cmd, ((launchcount) / 256) + 1, 1, 1);
        }

        VkBufferMemoryBarrier barrier = vkslime::buffer_barrier(mRenderScene.objectDataBuffer.mBuffer, mGraphicsQueueFamily);
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        uploadBarriers.push_back(barrier);
        mRenderScene.clear_dirty_objects();
    }

    RenderScene::MeshPass* passes[3] = { &mRenderScene._forwardPass,&mRenderScene._transparentForwardPass,&mRenderScene._shadowPass };
    for (int p = 0; p < 3; p++)
    {
        auto& pass = *passes[p];


        //reallocate the gpu side buffers if needed

        if (pass.drawIndirectBuffer.mSize < pass.batches.size() * sizeof(GPUIndirectObject))
        {
            reallocate_buffer(pass.drawIndirectBuffer, pass.batches.size() * sizeof(GPUIndirectObject), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        }

        if (pass.compactedInstanceBuffer.mSize < pass.flat_batches.size() * sizeof(uint32_t))
        {
            reallocate_buffer(pass.compactedInstanceBuffer, pass.flat_batches.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        }

        if (pass.passObjectsBuffer.mSize < pass.flat_batches.size() * sizeof(GPUInstance))
        {
            reallocate_buffer(pass.passObjectsBuffer, pass.flat_batches.size() * sizeof(GPUInstance), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        }
    }

    std::vector<std::future<void>> async_calls;
    async_calls.reserve(9);

    std::vector<AllocatedBufferUntyped> unmaps;


    for (int p = 0; p < 3; p++)
    {
        RenderScene::MeshPass& pass = *passes[p];
        RenderScene::MeshPass* ppass = passes[p];

        RenderScene* pScene = &mRenderScene;
        //if the pass has changed the batches, need to reupload them
        if (pass.needsIndirectRefresh && pass.batches.size() > 0)
        {
            ZoneScopedNC("Refresh Indirect Buffer", tracy::Color::Red);

            AllocatedBuffer<GPUIndirectObject> newBuffer = create_buffer(sizeof(GPUIndirectObject) * pass.batches.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            GPUIndirectObject* indirect = map_buffer(newBuffer);



            async_calls.push_back(std::async(std::launch::async, [=] {


                pScene->fill_indirectArray(indirect, *ppass);

            }));
            //async_calls.push_back([&]() {
            //	_renderScene.fill_indirectArray(indirect, pass);
            //});

            unmaps.push_back(newBuffer);

            //unmap_buffer(newBuffer);

            if (pass.clearIndirectBuffer.mBuffer != VK_NULL_HANDLE)
            {
                AllocatedBufferUntyped deletionBuffer = pass.clearIndirectBuffer;
                //add buffer to deletion queue of this frame
                get_current_frame()._frameDeletionQueue.push_function([=]() {

                    vmaDestroyBuffer(mAllocator, deletionBuffer.mBuffer, deletionBuffer.mAllocation);
                });
            }

            pass.clearIndirectBuffer = newBuffer;
            pass.needsIndirectRefresh = false;
        }

        if (pass.needsInstanceRefresh && !pass.flat_batches.empty())
        {
            ZoneScopedNC("Refresh Instancing Buffer", tracy::Color::Red);

            AllocatedBuffer<GPUInstance> newBuffer = create_buffer(sizeof(GPUInstance) * pass.flat_batches.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            GPUInstance* instanceData = map_buffer(newBuffer);
            async_calls.push_back(std::async(std::launch::async, [=] {


                pScene->fill_instancesArray(instanceData, *ppass);

            }));
            unmaps.push_back(newBuffer);
            //_renderScene.fill_instancesArray(instanceData, pass);
            //unmap_buffer(newBuffer);

            get_current_frame()._frameDeletionQueue.push_function([=]() {

                vmaDestroyBuffer(mAllocator, newBuffer.mBuffer, newBuffer.mAllocation);
            });

            //copy from the uploaded cpu side instance buffer to the gpu one
            VkBufferCopy indirectCopy;
            indirectCopy.dstOffset = 0;
            indirectCopy.size = pass.flat_batches.size() * sizeof(GPUInstance);
            indirectCopy.srcOffset = 0;
            vkCmdCopyBuffer(cmd, newBuffer.mBuffer, pass.passObjectsBuffer.mBuffer, 1, &indirectCopy);

            VkBufferMemoryBarrier barrier = vkslime::buffer_barrier(pass.passObjectsBuffer.mBuffer, mGraphicsQueueFamily);
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            uploadBarriers.push_back(barrier);

            pass.needsInstanceRefresh = false;
        }
    }

    for (auto& s : async_calls)
    {
        s.get();
    }
    for (auto b : unmaps)
    {
        unmap_buffer(b);
    }

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, static_cast<uint32_t>(uploadBarriers.size()), uploadBarriers.data(), 0, nullptr);//1, &readBarrier);
    uploadBarriers.clear();
}

void VulkanEngine::draw_objects_forward(VkCommandBuffer cmd, RenderScene::MeshPass& pass)
{
    ZoneScopedNC("DrawObjects", tracy::Color::Blue);
    //make a model view matrix for rendering the object
    //camera view
    glm::mat4 view = mCamera.get_view_matrix();
    //camera projection
    glm::mat4 projection = mCamera.get_projection_matrix();


    GPUCameraData camData;
    camData.proj = projection;
    camData.view = view;
    camData.viewproj = projection * view;

    mSceneParameters.sunlightShadowMatrix = mMainLight.get_projection() * mMainLight.get_view();


    float framed = (mFrameNumber / 120.f);
    mSceneParameters.ambientColor = glm::vec4{ 0.5 };
    mSceneParameters.sunlightColor = glm::vec4{ 1.f };
    mSceneParameters.sunlightDirection = glm::vec4(mMainLight.lightDirection * 1.f,1.f);

    mSceneParameters.sunlightColor.w = 1;

    //push data to dynmem
    uint32_t scene_data_offset = get_current_frame().dynamicData.push(mSceneParameters);

    uint32_t camera_data_offset = get_current_frame().dynamicData.push(camData);


    VkDescriptorBufferInfo objectBufferInfo = mRenderScene.objectDataBuffer.get_info();

    VkDescriptorBufferInfo sceneInfo = get_current_frame().dynamicData.source.get_info();
    sceneInfo.range = sizeof(GPUSceneData);

    VkDescriptorBufferInfo camInfo = get_current_frame().dynamicData.source.get_info();
    camInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo instanceInfo = pass.compactedInstanceBuffer.get_info();


    VkDescriptorImageInfo shadowImage;
    shadowImage.sampler = mShadowSampler;

    shadowImage.imageView = mShadowImage.mDefaultView;
    shadowImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorSet GlobalSet;
    vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
            .bind_buffer(0, &camInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_buffer(1, &sceneInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT| VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(2, &shadowImage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(GlobalSet);

    VkDescriptorSet ObjectDataSet;
    vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
            .bind_buffer(0, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_buffer(1, &instanceInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build(ObjectDataSet);
    vkCmdSetDepthBias(cmd, 0, 0, 0);

    std::vector<uint32_t> dynamic_offsets;
    dynamic_offsets.push_back(camera_data_offset);
    dynamic_offsets.push_back(scene_data_offset);
    execute_draw_commands(cmd, pass, ObjectDataSet, dynamic_offsets, GlobalSet);
}


void VulkanEngine::execute_draw_commands(VkCommandBuffer cmd, RenderScene::MeshPass& pass, VkDescriptorSet ObjectDataSet, std::vector<uint32_t> dynamic_offsets, VkDescriptorSet GlobalSet)
{
    if(!pass.batches.empty())
    {
        ZoneScopedNC("Draw Commit", tracy::Color::Blue4);
        Mesh* lastMesh = nullptr;
        VkPipeline lastPipeline{ VK_NULL_HANDLE };
        VkPipelineLayout lastLayout{ VK_NULL_HANDLE };
        VkDescriptorSet lastMaterialSet{ VK_NULL_HANDLE };

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &mRenderScene.mergedVertexBuffer.mBuffer, &offset);

        vkCmdBindIndexBuffer(cmd, mRenderScene.mergedIndexBuffer.mBuffer, 0, VK_INDEX_TYPE_UINT32);

        stats.objects = static_cast<uint32_t>(pass.flat_batches.size());
        for (int i = 0; i < pass.multibatches.size(); i++)
        {
            auto& multibatch = pass.multibatches[i];
            auto& instanceDraw = pass.batches[multibatch.first];

            VkPipeline newPipeline = instanceDraw.material.shaderPass->pipeline;
            VkPipelineLayout newLayout = instanceDraw.material.shaderPass->layout;
            VkDescriptorSet newMaterialSet = instanceDraw.material.materialSet;

            Mesh* drawMesh = mRenderScene.get_mesh(instanceDraw.meshID)->original;

            if (newPipeline != lastPipeline)
            {
                lastPipeline = newPipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newPipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newLayout, 1, 1, &ObjectDataSet, 0, nullptr);

                //update dynamic binds
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newLayout, 0, 1, &GlobalSet, dynamic_offsets.size(), dynamic_offsets.data());
            }
            if (newMaterialSet != lastMaterialSet)
            {
                lastMaterialSet = newMaterialSet;
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newLayout, 2, 1, &newMaterialSet, 0, nullptr);
            }

            bool merged = mRenderScene.get_mesh(instanceDraw.meshID)->isMerged;
            if (merged)
            {
                if (lastMesh != nullptr)
                {
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(cmd, 0, 1, &mRenderScene.mergedVertexBuffer.mBuffer, &offset);

                    vkCmdBindIndexBuffer(cmd, mRenderScene.mergedIndexBuffer.mBuffer, 0, VK_INDEX_TYPE_UINT32);
                    lastMesh = nullptr;
                }
            }
            else if (lastMesh != drawMesh) {

                //bind the mesh vertex buffer with offset 0
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmd, 0, 1, &drawMesh->mVertexBuffer.mBuffer, &offset);

                if (drawMesh->mIndexBuffer.mBuffer != VK_NULL_HANDLE) {
                    vkCmdBindIndexBuffer(cmd, drawMesh->mIndexBuffer.mBuffer, 0, VK_INDEX_TYPE_UINT32);
                }
                lastMesh = drawMesh;
            }

            bool bHasIndices = drawMesh->mIndices.size() > 0;
            if (!bHasIndices) {
                stats.draws++;
                stats.triangles += static_cast<int32_t>(drawMesh->mVertices.size() / 3) * instanceDraw.count;
                vkCmdDraw(cmd, static_cast<uint32_t>(drawMesh->mVertices.size()), instanceDraw.count, 0, instanceDraw.first);
            }
            else {
                stats.triangles += static_cast<int32_t>(drawMesh->mIndices.size() / 3) * instanceDraw.count;

                vkCmdDrawIndexedIndirect(cmd, pass.drawIndirectBuffer.mBuffer, multibatch.first * sizeof(GPUIndirectObject), multibatch.count, sizeof(GPUIndirectObject));

                stats.draws++;
                stats.drawcalls += instanceDraw.count;
            }
        }
    }
}
void VulkanEngine::draw_objects_shadow(VkCommandBuffer cmd, RenderScene::MeshPass& pass)
{
    ZoneScopedNC("DrawObjects", tracy::Color::Blue);

    glm::mat4 view = mMainLight.get_view();

    glm::mat4 projection = mMainLight.get_projection();

    GPUCameraData camData;
    camData.proj = projection;
    camData.view = view;
    camData.viewproj = projection * view;

    //push data to dynmem
    uint32_t camera_data_offset = get_current_frame().dynamicData.push(camData);


    VkDescriptorBufferInfo objectBufferInfo = mRenderScene.objectDataBuffer.get_info();

    VkDescriptorBufferInfo camInfo = get_current_frame().dynamicData.source.get_info();
    camInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo instanceInfo = pass.compactedInstanceBuffer.get_info();


    VkDescriptorSet GlobalSet;
    vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
            .bind_buffer(0, &camInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
            .build(GlobalSet);

    VkDescriptorSet ObjectDataSet;
    vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
            .bind_buffer(0, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_buffer(1, &instanceInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build(ObjectDataSet);



    vkCmdSetDepthBias(cmd,  5.25f, 0, 4.75f);

    std::vector<uint32_t> dynamic_offsets;
    dynamic_offsets.push_back(camera_data_offset);

    execute_draw_commands(cmd, pass, ObjectDataSet, dynamic_offsets, GlobalSet);
}



struct alignas(16) DepthReduceData
{
    glm::vec2 imageSize;
};
inline uint32_t getGroupCount(uint32_t threadCount, uint32_t localSize)
{
    return (threadCount + localSize - 1) / localSize;
}

void VulkanEngine::reduce_depth(VkCommandBuffer cmd)
{
    vkutil::VulkanScopeTimer timer(cmd, mProfiler, "Depth Reduce");

    VkImageMemoryBarrier depthReadBarriers[] =
            {
                    vkslime::image_barrier(mDepthImage.mImage, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
            };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, depthReadBarriers);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mDepthReducePipeline);

    for (int32_t i = 0; i < depthPyramidLevels; ++i)
    {
        VkDescriptorImageInfo destTarget;
        destTarget.sampler = mDepthSampler;
        destTarget.imageView = mDepthPyramidMips[i];
        destTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo sourceTarget;
        sourceTarget.sampler = mDepthSampler;
        if (i == 0)
        {
            sourceTarget.imageView = mDepthImage.mDefaultView;
            sourceTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        else {
            sourceTarget.imageView = mDepthPyramidMips[i - 1];
            sourceTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        VkDescriptorSet depthSet;
        vkutil::DescriptorBuilder::begin(mDescriptorLayoutCache, get_current_frame().dynamicDescriptorAllocator)
                .bind_image(0, &destTarget, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                .bind_image(1, &sourceTarget, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .build(depthSet);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mDepthReduceLayout, 0, 1, &depthSet, 0, nullptr);

        uint32_t levelWidth = depthPyramidWidth >> i;
        uint32_t levelHeight = depthPyramidHeight >> i;
        if (levelHeight < 1) levelHeight = 1;
        if (levelWidth < 1) levelWidth = 1;

        DepthReduceData reduceData = { glm::vec2(levelWidth, levelHeight) };

        vkCmdPushConstants(cmd, mDepthReduceLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduceData), &reduceData);
        vkCmdDispatch(cmd, getGroupCount(levelWidth, 32), getGroupCount(levelHeight, 32), 1);


        VkImageMemoryBarrier reduceBarrier = vkslime::image_barrier(mDepthPyramid.mImage, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &reduceBarrier);
    }

    VkImageMemoryBarrier depthWriteBarrier = vkslime::image_barrier(mDepthImage.mImage, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &depthWriteBarrier);

}
