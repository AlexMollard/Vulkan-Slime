//
// Created by alexm on 23/12/2021.
//
#define VMA_IMPLEMENTATION

#include "VulkanEngine.h"
#include "VulkanTextures.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

#include <glm/gtx/transform.hpp>

namespace fs = std::filesystem;

//we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                     \
    do                                                                  \
    {                                                                   \
        VkResult err = x;                                               \
        if (err)                                                        \
        {                                                               \
            std::cout <<"Detected Vulkan error: " << err << std::endl;  \
            abort();                                                    \
        }                                                               \
    } while (0)

void VulkanEngine::init() {
    // Get current project path for file reading
    mCurrentProjectPath = std::filesystem::current_path().string();

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

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

    //load the core Vulkan structures
    init_vulkan();

    //create the swapchain
    init_swapchain();

    //Create Command Pools and command buffers
    init_commands();

    //Create render-pass
    init_default_renderpass();

    //Create the frame buffers
    init_framebuffers();

    //Create Fences and semaphores
    init_sync_structures();

    init_descriptors();

    init_pipeline();

    load_images();

    load_meshes();

    init_scene();

    init_imgui();

    layer.init();

    //everything went fine
    mIsInitialized = true;
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

    vkb::Instance vkb_inst = inst_ret.value();

    //store the instance
    mInstance = vkb_inst.instance;
    //store the debug messenger
    mDebugMessenger = vkb_inst.debug_messenger;

    // get the surface of the window we opened with SDL
    SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

    //use vkbootstrap to select a GPU.
    //We want a GPU that can write to the SDL surface and supports Vulkan 1.2
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physicalDevice = selector
            .set_minimum_version(1, 2)
            .set_surface(mSurface)
            .add_required_extension("VK_KHR_shader_draw_parameters")
            .select()
            .value();

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

    mMainDeletionQueue.push_function([this]() {
        vmaDestroyAllocator(mAllocator);
    });

    mGpuProperties = vkbDevice.physical_device.properties;
}

void VulkanEngine::init_swapchain() {
    vkb::SwapchainBuilder swapchainBuilder{mChosenGPU, mDevice, mSurface};

    // This is getting the total amout of supported framebuffer format types
    uint32_t formatCount = 0;
    auto result = vkGetPhysicalDeviceSurfaceFormatsKHR(mChosenGPU,mSurface,&formatCount,nullptr);
    assert(result == VK_SUCCESS);
    assert(formatCount >= 1);

    // This is getting all the framebuffer format types and putting them in a vector for later use
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            mChosenGPU, mSurface, &formatCount, surfaceFormats.data());
    assert(result == VK_SUCCESS);

    auto sRGBFormat = VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR  };
    auto adobeRGBFormat = VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT  };
    bool adobeRGBfound = false;
    for (auto format : surfaceFormats) {
        if(format.colorSpace == adobeRGBFormat.colorSpace)
            adobeRGBfound = true;
    }


    vkb::Swapchain vkbSwapchain = swapchainBuilder
            .use_default_format_selection()
                    //use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(mWindowExtent.width, mWindowExtent.height)
            .set_desired_format((adobeRGBfound) ? adobeRGBFormat : sRGBFormat)
            .build()
            .value();

    //store swapchain and its related images
    mSwapchain = vkbSwapchain.swapchain;
    mSwapchainImages = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    mSwapchainImageFormat = vkbSwapchain.image_format;

    std::cout << "Current swapchain framebuffer colour format: " << mSwapchainImageFormat <<
    " Lookup table, " << "https://tinyurl.com/bdfz9u6v" << std::endl;

    //depth image size will match the window
    VkExtent3D depthImageExtent = {
            mWindowExtent.width,
            mWindowExtent.height,
            1
    };

    //hardcoding the depth format to 32 bit float
    mDepthFormat = VK_FORMAT_D32_SFLOAT;

    //the depth image will be an image with the format we selected and Depth Attachment usage flag
    VkImageCreateInfo dimg_info = vkinit::image_create_info(mDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                            depthImageExtent);

    //for the depth image, we want to allocate it from GPU local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(mAllocator, &dimg_info, &dimg_allocinfo, &mDepthImage.mImage, &mDepthImage.mAllocation, nullptr);

    //build an image-view for the depth image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(mDepthFormat, mDepthImage.mImage,
                                                                     VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(mDevice, &dview_info, nullptr, &mDepthImageView));

    mMainDeletionQueue.push_function([this]() {
        vkDestroyImageView(mDevice, mDepthImageView, nullptr);
        vmaDestroyImage(mAllocator, mDepthImage.mImage, mDepthImage.mAllocation);
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    });
}

void VulkanEngine::init_commands() {
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(mGraphicsQueueFamily,
                                                                               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto &frame: mFrames) {
        VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &frame.mCommandPool));

        //allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frame.mCommandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &frame.mMainCommandBuffer));

        mMainDeletionQueue.push_function([this, frame]() {
            vkDestroyCommandPool(mDevice, frame.mCommandPool, nullptr);
        });
    }

    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(mGraphicsQueueFamily);
//create pool for upload context
    VK_CHECK(vkCreateCommandPool(mDevice, &uploadCommandPoolInfo, nullptr, &mUploadContext.mCommandPool));

    mMainDeletionQueue.push_function([this]() {
        vkDestroyCommandPool(mDevice, mUploadContext.mCommandPool, nullptr);
    });

    //allocate the default command buffer that we will use for the instant commands
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(mUploadContext.mCommandPool, 1);

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mUploadContext.mCommandBuffer));


}

void VulkanEngine::cleanup() {
    if (mIsInitialized) {
        //make sure the GPU has stopped doing its things
        vkDeviceWaitIdle(mDevice);

        mMainDeletionQueue.flush();

        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

        vkDestroyDevice(mDevice, nullptr);
        vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
        vkDestroyInstance(mInstance, nullptr);

        SDL_DestroyWindow(mWindow);
    }
}

void VulkanEngine::draw() {
    ImGui::Render();

    //wait until the GPU has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(mDevice, 1, &get_current_frame().mRenderFence, true, 1000000000));
    VK_CHECK(vkResetFences(mDevice, 1, &get_current_frame().mRenderFence));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(get_current_frame().mMainCommandBuffer, 0));

    //request image from the swapchain, one second timeout
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(mDevice, mSwapchain, 1000000000, get_current_frame().mPresentSemaphore, nullptr,
                                   &swapchainImageIndex));

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame().mMainCommandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //make a clear-color from frame number. This will flash with a 120*pi frame period.
    VkClearValue clearValue;
    clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

    //clear depth at 1
    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.f;

    //start the main renderpass.
    //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(mRenderPass, mWindowExtent,
                                                                 mFramebuffers[swapchainImageIndex]);

    //connect clear values
    rpInfo.clearValueCount = 2;

    VkClearValue clearValues[] = {clearValue, depthClear};

    rpInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    draw_objects(cmd, mRenderables.data(), (int) mRenderables.size());

    //ImGui::Te;

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    //finalize the render pass
    vkCmdEndRenderPass(cmd);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &waitStage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &get_current_frame().mPresentSemaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &get_current_frame().mRenderSemaphore;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(mGraphicsQueue, 1, &submit, get_current_frame().mRenderFence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame().mRenderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(mGraphicsQueue, &presentInfo));

    //increase the number of frames drawn
    mFrameNumber++;
}

void VulkanEngine::run() {
    SDL_Event e;
    bool shouldClose = false;

    //main loop
    while (!shouldClose) {
        //Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&e);

            //close the window when user clicks the X button or alt-f4s
            if (e.type == SDL_QUIT) shouldClose = true;
        }

        //imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(mWindow);

        ImGui::NewFrame();

        //imgui commands
        ImGui::ShowDemoWindow();
        layer.draw();
        draw();
        ImGui::EndFrame();
    }
}

void VulkanEngine::init_default_renderpass() {
    // the renderpass will use this color attachment.
    VkAttachmentDescription color_attachment = {};
    //the attachment will have the format needed by the swapchain
    color_attachment.format = mSwapchainImageFormat;
    //1 sample, we won't be doing MSAA
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // we Clear when this attachment is loaded
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // we keep the attachment stored when the renderpass ends
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //we don't care about stencil
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    //we don't know or care about the starting layout of the attachment
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    //after the renderpass ends, the image has to be on a layout ready for display
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    //attachment number will index into the pAttachments array in the parent renderpass itself
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

    //dependency from outside to the subpass, making this subpass dependent on the previous renderpasses
    VkSubpassDependency depth_dependency = {};
    depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depth_dependency.dstSubpass = 0;
    depth_dependency.srcStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.srcAccessMask = 0;
    depth_dependency.dstStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //array of 2 dependencies, one for color, two for depth
    VkSubpassDependency dependencies[2] = {dependency, depth_dependency};

    //array of 2 attachments, one for the color, and other for depth
    VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};


    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    //2 attachments from attachment array
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = &attachments[0];
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    //2 dependencies from dependency array
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = &dependencies[0];

    VK_CHECK(vkCreateRenderPass(mDevice, &render_pass_info, nullptr, &mRenderPass));

    mMainDeletionQueue.push_function([this]() {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    });
}

void VulkanEngine::init_framebuffers() {
    //create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
    VkFramebufferCreateInfo fb_info = vkinit::framebuffer_create_info(mRenderPass, mWindowExtent);

    const auto swapchain_imagecount = (uint32_t) mSwapchainImages.size();
    mFramebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

    for (int i = 0; i < swapchain_imagecount; i++) {

        VkImageView attachments[2];
        attachments[0] = mSwapchainImageViews[i];
        attachments[1] = mDepthImageView;

        fb_info.pAttachments = attachments;
        fb_info.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(mDevice, &fb_info, nullptr, &mFramebuffers[i]));

        mMainDeletionQueue.push_function([this, i]() {
            vkDestroyFramebuffer(mDevice, mFramebuffers[i], nullptr);
            vkDestroyImageView(mDevice, mSwapchainImageViews[i], nullptr);
        });
    }
}

void VulkanEngine::init_sync_structures() {
    VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();
    VK_CHECK(vkCreateFence(mDevice, &uploadFenceCreateInfo, nullptr, &mUploadContext.mUploadFence));
    mMainDeletionQueue.push_function([this]() {
        vkDestroyFence(mDevice, mUploadContext.mUploadFence, nullptr);
    });

    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (auto &frame: mFrames) {

        VK_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &frame.mRenderFence));

        //enqueue the destruction of the fence
        mMainDeletionQueue.push_function([this, &frame]() {
            vkDestroyFence(mDevice, frame.mRenderFence, nullptr);
        });


        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &frame.mPresentSemaphore));
        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &frame.mRenderSemaphore));

        //enqueue the destruction of semaphores
        mMainDeletionQueue.push_function([this, &frame]() {
            vkDestroySemaphore(mDevice, frame.mPresentSemaphore, nullptr);
            vkDestroySemaphore(mDevice, frame.mRenderSemaphore, nullptr);
        });
    }
}


VkShaderModule VulkanEngine::load_shader_module(const char *filePath) {
    //open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    std::string fileName = fs::path(filePath).stem().generic_string();

    if (!file.is_open()) {
        return {};
    }

//find what the size of the file is by looking up the location of the cursor
//because the cursor is at the end, it gives the size directly in bytes
    std::streamsize fileSize = file.tellg();

//spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

//put file cursor at beginning
    file.seekg(0);

//load the entire file into the buffer
    file.read((char *) buffer.data(), fileSize);

//now that the file is loaded into the buffer, we can close it
    file.close();


//create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

//codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

//check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return {};
    }
    mMainDeletionQueue.push_function([this, shaderModule]() {
        vkDestroyShaderModule(mDevice, shaderModule, nullptr);
    });

    return shaderModule;
}

void VulkanEngine::init_pipeline() {
    //build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder{};

    pipelineBuilder.mShaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      load_shader_module("../Res/Shaders/lit.vert.spv")));

    pipelineBuilder.mShaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      load_shader_module("../Res/Shaders/lit.frag.spv")));


    //we start from just the default empty pipeline layout info
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

    //setup push constants
    VkPushConstantRange push_constant;
    push_constant.offset = 0;
    push_constant.size = sizeof(MeshPushConstants);
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
    mesh_pipeline_layout_info.pushConstantRangeCount = 1;

    VkDescriptorSetLayout setLayouts[] = {mGlobalSetLayout, mObjectSetLayout, mSingleTextureSetLayout};
    mesh_pipeline_layout_info.setLayoutCount = 3;
    mesh_pipeline_layout_info.pSetLayouts = setLayouts;

    VkPipelineLayout meshPipLayout;
    VK_CHECK(vkCreatePipelineLayout(mDevice, &mesh_pipeline_layout_info, nullptr, &meshPipLayout));

    //use the mesh layout we created
    pipelineBuilder.mPipelineLayout = meshPipLayout;
    pipelineBuilder.mVertexInputInfo = vkinit::vertex_input_state_create_info();
    pipelineBuilder.mInputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    //build viewport and scissor from the swapchain extents
    pipelineBuilder.mViewport.x = 0.0f;
    pipelineBuilder.mViewport.y = 0.0f;
    pipelineBuilder.mViewport.width = (float) mWindowExtent.width;
    pipelineBuilder.mViewport.height = (float) mWindowExtent.height;
    pipelineBuilder.mViewport.minDepth = 0.0f;
    pipelineBuilder.mViewport.maxDepth = 1.0f;

    pipelineBuilder.mScissor.offset = {0, 0};
    pipelineBuilder.mScissor.extent = mWindowExtent;

    pipelineBuilder.mRasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
    pipelineBuilder.mMultisampling = vkinit::multisampling_state_create_info();
    pipelineBuilder.mColorBlendAttachment = vkinit::color_blend_attachment_state();

    pipelineBuilder.mDepthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    //build the mesh pipeline
    VertexInputDescription vertexDescription = Vertex::get_vertex_description();

    //connect the pipeline builder vertex input info to the one we get from Vertex
    pipelineBuilder.mVertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pipelineBuilder.mVertexInputInfo.vertexAttributeDescriptionCount = (int) vertexDescription.attributes.size();

    pipelineBuilder.mVertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pipelineBuilder.mVertexInputInfo.vertexBindingDescriptionCount = (int) vertexDescription.bindings.size();


    VkPipeline meshPipeline = pipelineBuilder.build_pipeline(mDevice, mRenderPass);
    create_material(meshPipeline, meshPipLayout, std::string_view{"defaultMesh"});

    mMainDeletionQueue.push_function([this, meshPipeline, meshPipLayout]() {
        vkDestroyPipeline(mDevice, meshPipeline, nullptr);

        //destroy the pipeline layout that they use
        vkDestroyPipelineLayout(mDevice, meshPipLayout, nullptr);
    });
}

void VulkanEngine::load_meshes() {
    std::string lostEmpirePath = std::string(mCurrentProjectPath + std::string("/../assets/Models/lost-empire/lost_empire.obj"));
    Mesh lostEmpire{};
    lostEmpire.load_from_obj(lostEmpirePath.c_str());

    upload_mesh(lostEmpire);

    mMeshes["empire"] = lostEmpire;
}

void VulkanEngine::upload_mesh(Mesh &mesh) {
    const size_t bufferSize = mesh.mVertices.size() * sizeof(Vertex);
    //allocate vertex buffer
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    stagingBufferInfo.size = bufferSize;
    //this buffer is going to be used as a Vertex Buffer
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;


    //let the VMA library know that this data should be writeable by CPU, but also readable by GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    vktype::AllocatedBuffer stagingBuffer{};

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(mAllocator, &stagingBufferInfo, &vmaallocInfo,
                             &stagingBuffer.mBuffer,
                             &stagingBuffer.mAllocation,
                             nullptr));

    //copy vertex data
    void *data;
    vmaMapMemory(mAllocator, stagingBuffer.mAllocation, &data);

    memcpy(data, mesh.mVertices.data(), mesh.mVertices.size() * sizeof(Vertex));

    vmaUnmapMemory(mAllocator, stagingBuffer.mAllocation);


    //allocate vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    vertexBufferInfo.size = bufferSize;
    //this buffer is going to be used as a Vertex Buffer
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    //let the VMA library know that this data should be gpu native
    vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(mAllocator, &vertexBufferInfo, &vmaallocInfo,
                             &mesh.mVertexBuffer.mBuffer,
                             &mesh.mVertexBuffer.mAllocation,
                             nullptr));
    //add the destruction of triangle mesh buffer to the deletion queue
    mMainDeletionQueue.push_function([=]() {

        vmaDestroyBuffer(mAllocator, mesh.mVertexBuffer.mBuffer, mesh.mVertexBuffer.mAllocation);
    });

    immediate_submit([=](VkCommandBuffer cmd) {
        VkBufferCopy copy;
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.mBuffer, mesh.mVertexBuffer.mBuffer, 1, &copy);
    });

    vmaDestroyBuffer(mAllocator, stagingBuffer.mBuffer, stagingBuffer.mAllocation);
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass) {
    //make viewport state from our stored viewport and scissor.
    //at the moment we won't support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.pViewports = &mViewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &mScissor;

    //setup dummy color blending. We aren't using transparent objects yet
    //the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &mColorBlendAttachment;

    //build the actual pipeline
    //we now use all of the info structs we have been writing into into this one to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;

    pipelineInfo.stageCount = (uint32_t) mShaderStages.size();
    pipelineInfo.pStages = mShaderStages.data();
    pipelineInfo.pVertexInputState = &mVertexInputInfo;
    pipelineInfo.pInputAssemblyState = &mInputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &mRasterizer;
    pipelineInfo.pMultisampleState = &mMultisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &mDepthStencil;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.renderPass = pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    //it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        std::cout << "failed to create pipeline\n";
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    } else {
        return newPipeline;
    }
}

Material *VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string_view &name) {
    Material mat{};
    mat.pipeline = pipeline;
    mat.pipelineLayout = layout;
    mMaterials[name] = mat;
    return &mMaterials[name];
}

Material *VulkanEngine::get_material(const std::string_view &name) {
    //Search for the object, and return nullptr if not found
    auto it = mMaterials.find(name);
    if (it == mMaterials.end()) {
        std::cout << "FAILED TO GET MATERIAL: " << name << std::endl;
        return nullptr;
    } else
        return &(*it).second;
}

Mesh *VulkanEngine::get_mesh(const std::string_view &name) {
    //Search for the object, and return nullptr if not found
    auto it = mMeshes.find(name);
    if (it == mMeshes.end()) {
        std::cout << "FAILED TO GET MESH: " << name << std::endl;
        return nullptr;
    } else
        return &(*it).second;
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject *first, int count) {
    //Make a model view matrix for rendering the object
    // Camera view
    glm::vec3 camPos = {0.0f, -6.0f, glm::cos((float) -mFrameNumber * 0.001f) * 85.0f};
    glm::mat4 view = glm::translate(glm::mat4{1.0f}, camPos);

    //Camera Projection
    glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f);
    projection[1][1] *= -1; // Flip camera on UP axis

    //fill a GPU camera data struct
    GPUCameraData camData{};
    camData.proj = projection;
    camData.view = view;
    camData.viewproj = projection * view;

    //and copy it to the buffer
    void *data;
    vmaMapMemory(mAllocator, get_current_frame().cameraBuffer.mAllocation, &data);

    memcpy(data, &camData, sizeof(GPUCameraData));

    vmaUnmapMemory(mAllocator, get_current_frame().cameraBuffer.mAllocation);

    // Scene Parameters
    float framed = ((float) mFrameNumber / 120.f);

    mSceneParameters.ambientColour = {sin(framed), 0, cos(framed), 1};

    char *sceneData;
    vmaMapMemory(mAllocator, mSceneParameterBuffer.mAllocation, (void **) &sceneData);

    int frameIndex = mFrameNumber % FRAME_OVERLAP;

    sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;

    memcpy(sceneData, &mSceneParameters, sizeof(GPUSceneData));

    vmaUnmapMemory(mAllocator, mSceneParameterBuffer.mAllocation);

    void *objectData;
    vmaMapMemory(mAllocator, get_current_frame().objectBuffer.mAllocation, &objectData);

    auto *objectSSBO = (GPUObjectData *) objectData;

    for (int i = 0; i < count; i++) {
        RenderObject const &object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }

    vmaUnmapMemory(mAllocator, get_current_frame().objectBuffer.mAllocation);

    Mesh const *lastMesh = nullptr;
    Material const *lastMaterial = nullptr;
    for (int i = 0; i < count; ++i) {
        RenderObject &object = first[i];

        //only bind the pipeline if it doesn't math the already bound one
        if (object.material != lastMaterial) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
            lastMaterial = object.material;

            //offset for our scene buffer
            uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
            //bind the descriptor set when changing pipeline
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1,
                                    &get_current_frame().globalDescriptor, 1, &uniform_offset);

            //object data descriptor
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1,
                                    &get_current_frame().objectDescriptor, 0, nullptr);

            if (object.material->textureSet != VK_NULL_HANDLE) {
                //texture descriptor
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1,
                                        &object.material->textureSet, 0, nullptr);
            }
        }

        MeshPushConstants constants{};
        constants.renderMatrix = object.transformMatrix;

        //Upload the mesh to the GPU via push constants
        vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(MeshPushConstants), &constants);

        //only bind mesh if it's a diffrent one from last bind
        if (object.mesh != lastMesh) {
            //bind mesh vertex buffer with offset 0
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->mVertexBuffer.mBuffer, &offset);
            lastMesh = object.mesh;
        }
        //We can now draw
        vkCmdDraw(cmd, (uint32_t) object.mesh->mVertices.size(), 1, 0, i);
    }

}

void VulkanEngine::init_scene() {
    RenderObject map{};
    map.mesh = get_mesh("empire");
    map.material = get_material("defaultMesh");

    glm::mat4 translation = glm::translate(glm::mat4{1.0f}, glm::vec3{5, -10, 0});
    map.transformMatrix = translation;

    mRenderables.push_back(map);

    //create a sampler for the texture
    VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

    VkSampler blockySampler;
    vkCreateSampler(mDevice, &samplerInfo, nullptr, &blockySampler);

    mMainDeletionQueue.push_function([this, blockySampler]() {
        vkDestroySampler(mDevice, blockySampler, nullptr);
    });

    Material *texturedMat = get_material("defaultMesh");

    //allocate the descriptor set for single-texture to use on the material
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mSingleTextureSetLayout;

    vkAllocateDescriptorSets(mDevice, &allocInfo, &texturedMat->textureSet);

    //write to the descriptor set so that it points to our empire_diffuse texture
    VkDescriptorImageInfo imageBufferInfo;
    imageBufferInfo.sampler = blockySampler;
    imageBufferInfo.imageView = mLoadedTextures["empire_diffuse"].imageView;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                   texturedMat->textureSet, &imageBufferInfo, 0);

    vkUpdateDescriptorSets(mDevice, 1, &texture1, 0, nullptr);
}

FrameData &VulkanEngine::get_current_frame() {
    return mFrames[mFrameNumber % FRAME_OVERLAP];
}

vktype::AllocatedBuffer
VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    //allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;

    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;


    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;

    vktype::AllocatedBuffer newBuffer{};

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(mAllocator, &bufferInfo, &vmaallocInfo,
                             &newBuffer.mBuffer,
                             &newBuffer.mAllocation,
                             nullptr));

    return newBuffer;
}

void VulkanEngine::init_descriptors() {
    //create a descriptor pool that will hold 10 uniform buffers
    std::vector<VkDescriptorPoolSize> sizes =
            {
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         10},
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         10},
                    //add combined-image-sampler descriptor types to the pool
                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}
            };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = 10;
    pool_info.poolSizeCount = (uint32_t) sizes.size();
    pool_info.pPoolSizes = sizes.data();

    vkCreateDescriptorPool(mDevice, &pool_info, nullptr, &mDescriptorPool);

    VkDescriptorSetLayoutBinding cameraBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                                   VK_SHADER_STAGE_VERTEX_BIT, 0);
    VkDescriptorSetLayoutBinding sceneBind = vkinit::descriptorset_layout_binding(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

    VkDescriptorSetLayoutBinding bindings[] = {cameraBind, sceneBind};

    VkDescriptorSetLayoutCreateInfo setinfo = {};
    setinfo.bindingCount = 2;
    setinfo.flags = 0;
    setinfo.pNext = nullptr;
    setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setinfo.pBindings = bindings;

    vkCreateDescriptorSetLayout(mDevice, &setinfo, nullptr, &mGlobalSetLayout);

    VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                                   VK_SHADER_STAGE_VERTEX_BIT, 0);

    VkDescriptorSetLayoutCreateInfo set2info = {};
    set2info.bindingCount = 1;
    set2info.flags = 0;
    set2info.pNext = nullptr;
    set2info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set2info.pBindings = &objectBind;

    vkCreateDescriptorSetLayout(mDevice, &set2info, nullptr, &mObjectSetLayout);

    VkDescriptorSetLayoutBinding textureBind = vkinit::descriptorset_layout_binding(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    VkDescriptorSetLayoutCreateInfo set3info = {};
    set3info.bindingCount = 1;
    set3info.flags = 0;
    set3info.pNext = nullptr;
    set3info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set3info.pBindings = &textureBind;

    vkCreateDescriptorSetLayout(mDevice, &set3info, nullptr, &mSingleTextureSetLayout);


    const size_t sceneParamBufferSize = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData));

    mSceneParameterBuffer = create_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VMA_MEMORY_USAGE_CPU_TO_GPU);

    for (auto &frame: mFrames) {
        frame.cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VMA_MEMORY_USAGE_CPU_TO_GPU);

        const int MAX_OBJECTS = 10000;
        frame.objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                           VMA_MEMORY_USAGE_CPU_TO_GPU);

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mGlobalSetLayout;

        vkAllocateDescriptorSets(mDevice, &allocInfo, &frame.globalDescriptor);

        VkDescriptorSetAllocateInfo objectSetAlloc = {};
        objectSetAlloc.pNext = nullptr;
        objectSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        objectSetAlloc.descriptorPool = mDescriptorPool;
        objectSetAlloc.descriptorSetCount = 1;
        objectSetAlloc.pSetLayouts = &mObjectSetLayout;

        vkAllocateDescriptorSets(mDevice, &objectSetAlloc, &frame.objectDescriptor);

        VkDescriptorBufferInfo cameraInfo;
        cameraInfo.buffer = frame.cameraBuffer.mBuffer;
        cameraInfo.offset = 0;
        cameraInfo.range = sizeof(GPUCameraData);

        VkDescriptorBufferInfo sceneInfo;
        sceneInfo.buffer = mSceneParameterBuffer.mBuffer;
        sceneInfo.offset = 0;
        sceneInfo.range = sizeof(GPUSceneData);

        VkDescriptorBufferInfo objectBufferInfo;
        objectBufferInfo.buffer = frame.objectBuffer.mBuffer;
        objectBufferInfo.offset = 0;
        objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;


        VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                           frame.globalDescriptor, &cameraInfo, 0);

        VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                                                          frame.globalDescriptor, &sceneInfo, 1);

        VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                           frame.objectDescriptor, &objectBufferInfo,
                                                                           0);

        VkWriteDescriptorSet setWrites[] = {cameraWrite, sceneWrite, objectWrite};

        vkUpdateDescriptorSets(mDevice, 3, setWrites, 0, nullptr);
    }

    mMainDeletionQueue.push_function([this]() {

        vmaDestroyBuffer(mAllocator, mSceneParameterBuffer.mBuffer, mSceneParameterBuffer.mAllocation);

        vkDestroyDescriptorSetLayout(mDevice, mObjectSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mGlobalSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mSingleTextureSetLayout, nullptr);

        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

        for (auto &frame: mFrames) {
            vmaDestroyBuffer(mAllocator, frame.cameraBuffer.mBuffer, frame.cameraBuffer.mAllocation);

            vmaDestroyBuffer(mAllocator, frame.objectBuffer.mBuffer, frame.objectBuffer.mAllocation);
        }
    });

}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize) {
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = mGpuProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function) {
    VkCommandBuffer cmd = mUploadContext.mCommandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //execute the function
    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = vkinit::submit_info(&cmd);


    //submit command buffer to the queue and execute it.
    // _uploadFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(mGraphicsQueue, 1, &submit, mUploadContext.mUploadFence));

    vkWaitForFences(mDevice, 1, &mUploadContext.mUploadFence, true, 9999999999);
    vkResetFences(mDevice, 1, &mUploadContext.mUploadFence);

    // reset the command buffers inside the command pool
    vkResetCommandPool(mDevice, mUploadContext.mCommandPool, 0);
}

void VulkanEngine::load_images() {
    Texture lostEmpire{};
    std::string lostEmpireImagePath = mCurrentProjectPath + "/../assets/Models/lost-empire/lost_empire-RGBA.png";
    vkutil::load_image_from_file(*this, lostEmpireImagePath.c_str(), lostEmpire.image);

    VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.image.mImage,
                                                                    VK_IMAGE_ASPECT_COLOR_BIT);
    vkCreateImageView(mDevice, &imageinfo, nullptr, &lostEmpire.imageView);

    mMainDeletionQueue.push_function([this, lostEmpire]() {
        vkDestroyImageView(mDevice, lostEmpire.imageView, nullptr);
    });


    mLoadedTextures["empire_diffuse"] = lostEmpire;
}

void VulkanEngine::init_imgui() {
//1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] =
            {
                    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(mDevice, &pool_info, nullptr, &imguiPool));


    // 2: initialize imgui library

    //this initializes the core structures of imgui
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    float fontSize = 18.0f;
    io.Fonts->AddFontFromFileTTF("C:/Users/alexm/CLionProjects/VulkanSlime/assets/Fonts/opensans/OpenSans-Bold.ttf", fontSize);
    io.FontDefault = io.Fonts->AddFontFromFileTTF("C:/Users/alexm/CLionProjects/VulkanSlime/assets/Fonts/opensans/OpenSans-Regular.ttf", fontSize);

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
    mMainDeletionQueue.push_function([=]() {

        vkDestroyDescriptorPool(mDevice, imguiPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
    });
}
