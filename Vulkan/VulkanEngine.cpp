//
// Created by alexm on 23/12/2021.
//
#define VMA_IMPLEMENTATION

#include "VulkanEngine.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#include <iostream>
#include <fstream>
#include <filesystem>

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

    init_pipeline();

    load_meshes();

    init_scene();

    //everything went fine
    mIsInitialized = true;
}

void VulkanEngine::init_vulkan() {
    vkb::InstanceBuilder builder;

    //make the Vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Slime Vulkan")
            .request_validation_layers(true)
            .require_api_version(1, 1, 0)
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
    //We want a GPU that can write to the SDL surface and supports Vulkan 1.1
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physicalDevice = selector
            .set_minimum_version(1, 1)
            .set_surface(mSurface)
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
}

void VulkanEngine::init_swapchain() {
    vkb::SwapchainBuilder swapchainBuilder{mChosenGPU, mDevice, mSurface};

    vkb::Swapchain vkbSwapchain = swapchainBuilder
            .use_default_format_selection()
                    //use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(mWindowExtent.width, mWindowExtent.height)
            .build()
            .value();

    //store swapchain and its related images
    mSwapchain = vkbSwapchain.swapchain;
    mSwapchainImages = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    mSwapchainImageFormat = vkbSwapchain.image_format;

    //depth image size will match the window
    VkExtent3D depthImageExtent = {
            mWindowExtent.width,
            mWindowExtent.height,
            1
    };

    //hardcoding the depth format to 32 bit float
    _depthFormat = VK_FORMAT_D32_SFLOAT;

    //the depth image will be an image with the format we selected and Depth Attachment usage flag
    VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                            depthImageExtent);

    //for the depth image, we want to allocate it from GPU local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(mAllocator, &dimg_info, &dimg_allocinfo, &_depthImage.mImage, &_depthImage.mAllocation, nullptr);

    //build an image-view for the depth image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthFormat, _depthImage.mImage,
                                                                     VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(mDevice, &dview_info, nullptr, &_depthImageView));

    mMainDeletionQueue.push_function([this]() {
        vkDestroyImageView(mDevice, _depthImageView, nullptr);
        vmaDestroyImage(mAllocator, _depthImage.mImage, _depthImage.mAllocation);
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    });
}

void VulkanEngine::init_commands() {
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(mGraphicsQueueFamily,
                                                                               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mCommandPool));

    //allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(mCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mMainCommandBuffer));

    mMainDeletionQueue.push_function([this]() {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    });
}

void VulkanEngine::cleanup() {
    if (mIsInitialized) {
        //make sure the GPU has stopped doing its things
        vkWaitForFences(mDevice, 1, &mRenderFence, true, 1000000000);

        mMainDeletionQueue.flush();

        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

        vkDestroyDevice(mDevice, nullptr);
        vkDestroyInstance(mInstance, nullptr);

        SDL_DestroyWindow(mWindow);
    }
}

void VulkanEngine::draw() {
    //wait until the GPU has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(mDevice, 1, &mRenderFence, true, 1000000000));
    VK_CHECK(vkResetFences(mDevice, 1, &mRenderFence));

    //request image from the swapchain, one second timeout
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(mDevice, mSwapchain, 1000000000, mPresentSemaphore, nullptr, &swapchainImageIndex));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(mMainCommandBuffer, 0));

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = mMainCommandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext = nullptr;

    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //make a clear-color from frame number. This will flash with a 120*pi frame period.
    VkClearValue clearValue;
    float flash = abs(sin((float) mFrameNumber / 120.f));
    clearValue.color = {{0.3f, flash * 0.5f, 0.3f, 1.0f}};

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

    draw_objects(cmd, mRenderables.data(), mRenderables.size());

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
    submit.pWaitSemaphores = &mPresentSemaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &mRenderSemaphore;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(mGraphicsQueue, 1, &submit, mRenderFence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &mRenderSemaphore;
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
            //close the window when user clicks the X button or alt-f4s
            if (e.type == SDL_QUIT) shouldClose = true;
        }

        draw();
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
    depth_attachment.format = _depthFormat;
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
        attachments[1] = _depthImageView;

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
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    VK_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mRenderFence));

    //enqueue the destruction of the fence
    mMainDeletionQueue.push_function([this]() {
        vkDestroyFence(mDevice, mRenderFence, nullptr);
    });

    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mPresentSemaphore));
    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mRenderSemaphore));

    //enqueue the destruction of semaphores
    mMainDeletionQueue.push_function([this]() {
        vkDestroySemaphore(mDevice, mPresentSemaphore, nullptr);
        vkDestroySemaphore(mDevice, mRenderSemaphore, nullptr);
    });
}

bool VulkanEngine::load_shader_module(const char *filePath, VkShaderModule *outShaderModule) const {
    //open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    std::string fileName = fs::path(filePath).stem().generic_string();

    if (!file.is_open()) {
        std::cout << fileName << " Failed to open" << std::endl;
        return false;
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
        std::cout << fileName << " Shader module failed to be created" << std::endl;
        return false;
    }
    *outShaderModule = shaderModule;

    std::cout << fileName << " Shader Module Created!" << std::endl;
    return true;
}

void VulkanEngine::init_pipeline() {
    //compile mesh vertex shader
    std::cout << "[CREATION: Shader Modules]" << std::endl;
    VkShaderModule triangleFragShader;
    load_shader_module("../Res/Shaders/triangle.frag.spv", &triangleFragShader);

    VkShaderModule triangleVertexShader;
    load_shader_module("../Res/Shaders/triangle.vert.spv", &triangleVertexShader);

    //build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;

    pipelineBuilder.mShaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));

    pipelineBuilder.mShaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));


    //we start from just the default empty pipeline layout info
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

    //setup push constants
    VkPushConstantRange push_constant;
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = sizeof(MeshPushConstants);
    //this push constant range is accessible only in the vertex shader
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
    mesh_pipeline_layout_info.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(mDevice, &mesh_pipeline_layout_info, nullptr, &mMeshPipelineLayout));

    //build the pipeline layout that controls the inputs/outputs of the shader
    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipeline_layout_info, nullptr, &mTrianglePipelineLayout));

    //vertex input controls how to read vertices from vertex buffers. We aren't using it yet
    pipelineBuilder.mVertexInputInfo = vkinit::vertex_input_state_create_info();

    //input assembly is the configuration for drawing triangle lists, strips, or individual points.
    //we are just going to draw triangle list
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

    //configure the rasterizer to draw filled triangles
    pipelineBuilder.mRasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    //we don't use multisampling, so just run the default one
    pipelineBuilder.mMultisampling = vkinit::multisampling_state_create_info();

    //a single blend attachment with no blending and writing to RGBA
    pipelineBuilder.mColorBlendAttachment = vkinit::color_blend_attachment_state();

    //use the mesh layout we created
    pipelineBuilder.mPipelineLayout = mMeshPipelineLayout;

    //build the mesh pipeline
    VertexInputDescription vertexDescription = Vertex::get_vertex_description();

    //connect the pipeline builder vertex input info to the one we get from Vertex
    pipelineBuilder.mVertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pipelineBuilder.mVertexInputInfo.vertexAttributeDescriptionCount = (int)vertexDescription.attributes.size();

    pipelineBuilder.mVertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pipelineBuilder.mVertexInputInfo.vertexBindingDescriptionCount = (int)vertexDescription.bindings.size();

    //clear the shader stages for the builder
    pipelineBuilder.mShaderStages.clear();

    //finally build the pipeline
    pipelineBuilder.mDepthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
    mMeshPipeline = pipelineBuilder.build_pipeline(mDevice, mRenderPass);


    VkPipeline meshPipeline = pipelineBuilder.build_pipeline(mDevice, mRenderPass);
    create_material(meshPipeline,mMeshPipelineLayout,"defaultMesh");

    //destroy all shader modules, outside of the queue
    vkDestroyShaderModule(mDevice, triangleVertexShader, nullptr);
    vkDestroyShaderModule(mDevice, triangleFragShader, nullptr);

    mMainDeletionQueue.push_function([this]() {
        vkDestroyPipeline(mDevice, mMeshPipeline, nullptr);

        //destroy the pipeline layout that they use
        vkDestroyPipelineLayout(mDevice, mTrianglePipelineLayout, nullptr);
        vkDestroyPipelineLayout(mDevice, mMeshPipelineLayout, nullptr);
    });
}

void VulkanEngine::load_meshes() {
    //make the array 3 vertices long
    mTriangleMesh.mVertices.resize(3);

    //vertex positions
    mTriangleMesh.mVertices[0].position = {1.f, 1.f, 0.0f};
    mTriangleMesh.mVertices[1].position = {-1.f, 1.f, 0.0f};
    mTriangleMesh.mVertices[2].position = {0.f, -1.f, 0.0f};

    //vertex colors, all green
    mTriangleMesh.mVertices[0].color = {0.f, 1.f, 0.0f}; //pure green
    mTriangleMesh.mVertices[1].color = {0.f, 1.f, 0.0f}; //pure green
    mTriangleMesh.mVertices[2].color = {0.f, 1.f, 0.0f}; //pure green

    mMonkeyMesh.load_from_obj("../assets/Models/suzanne/suzanne.obj");

    upload_mesh(mTriangleMesh);
    upload_mesh(mMonkeyMesh);

    mMeshes["monkey"] = mMonkeyMesh;
    mMeshes["triangle"] = mTriangleMesh;
}

void VulkanEngine::upload_mesh(Mesh &mesh) {
    //allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    //this is the total size, in bytes, of the buffer we are allocating
    bufferInfo.size = mesh.mVertices.size() * sizeof(Vertex);
    //this buffer is going to be used as a Vertex Buffer
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;


    //let the VMA library know that this data should be writeable by CPU, but also readable by GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(mAllocator, &bufferInfo, &vmaallocInfo,
                             &mesh.mVertexBuffer.mBuffer,
                             &mesh.mVertexBuffer.mAllocation,
                             nullptr));

    //add the destruction of triangle mesh buffer to the deletion queue
    mMainDeletionQueue.push_function([this, &mesh]() {

        vmaDestroyBuffer(mAllocator, mesh.mVertexBuffer.mBuffer, mesh.mVertexBuffer.mAllocation);
    });

    //copy vertex data
    void *data;
    vmaMapMemory(mAllocator, mesh.mVertexBuffer.mAllocation, &data);

    memcpy(data, mesh.mVertices.data(), mesh.mVertices.size() * sizeof(Vertex));

    vmaUnmapMemory(mAllocator, mesh.mVertexBuffer.mAllocation);
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

Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string &name) {
    Material mat{};
    mat.pipeline = pipeline;
    mat.pipelineLayout = layout;
    mMaterials[name] = mat;
    return &mMaterials[name];
}

Material* VulkanEngine::get_material(const std::string &name) {
    //Search for the object, and return nullptr if not found
    auto it = mMaterials.find(name);
    if (it == mMaterials.end())
        return nullptr;
    else
        return &(*it).second;
}

Mesh* VulkanEngine::get_mesh(const std::string &name) {
    //Search for the object, and return nullptr if not found
    auto it = mMeshes.find(name);
    if (it == mMeshes.end())
        return nullptr;
    else
        return &(*it).second;
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject *first, int count) {
    //Make a model view matrix for rendering the object
    // Camera view
    glm::vec3 camPos = {0.0f, -6.0f, -10.0f};
    glm::mat4 view = glm::translate(glm::mat4{1.0f}, camPos);

    //Camera Projection
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1700.0f / 900.0f, 0.1f, 200.0f);
    projection[1][1] += -1; // Flip camera on UP axis

    Mesh* lastMesh = nullptr;
    Material* lastMaterial = nullptr;
    for (int i = 0; i < count; ++i) {
        RenderObject& object = first[i];

        //only bind the pipeline if it doesn't math the already bound one
        if (object.material != lastMaterial)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
            lastMaterial = object.material;
        }

        glm::mat4 model = object.transformMatrix;

        //final render matrix, that we are calculating on the CPU
        glm::mat4 meshMatrix = projection * view * model;

        MeshPushConstants constants;
        constants.renderMatrix = meshMatrix;

        //Upload the mesh to the GPU via push constants
        vkCmdPushConstants(cmd,object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,0, sizeof(MeshPushConstants), &constants);

        //only bind mesh if it's a diffrent one from last bind
        if (object.mesh != lastMesh)
        {
            //bind mesh vertex buffer with offset 0
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->mVertexBuffer.mBuffer, &offset);
            lastMesh = object.mesh;
        }
        //We can now draw
        vkCmdDraw(cmd, object.mesh->mVertices.size(),1,0,0);
    }

}

void VulkanEngine::init_scene() {
    RenderObject monkey{};
    monkey.mesh = get_mesh("monkey");
    monkey.material = get_material("monkey");
    monkey.transformMatrix = glm::mat4{1.0f};

    mRenderables.push_back(monkey);

    for (int x = -10; x < 10; ++x) {
        for (int y = -10; y < 10; ++y) {
            RenderObject tri{};
            tri.mesh = get_mesh("triangle");
            tri.material = get_material("defaultmesh");

            glm::mat4 translation = glm::translate(glm::mat4{1.0f}, glm::vec3(x,0,y));
            glm::mat4 scale = glm::scale(glm::mat4{1.0f}, glm::vec3(0.2f));
            tri.transformMatrix = translation * scale;

            mRenderables.push_back(tri);
        }
    }
}
