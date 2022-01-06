//
// Created by alexm on 23/12/2021.
//

#pragma once

#include <vulkan/vulkan.h>
#include <deque>
#include <unordered_map>
#include <functional>
#include "VulkanInitializers.h"
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

#include "VulkanMesh.h"

#include <glm/glm.hpp>

struct Material{
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

struct RenderObject {
    Mesh* mesh;

    Material* material;

    glm::mat4 transformMatrix;
};

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 renderMatrix;
};

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()> &&function) {
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); //call the function
        }

        deletors.clear();
    }
};

class PipelineBuilder {
public:

    std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;
    VkPipelineVertexInputStateCreateInfo mVertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
    VkViewport mViewport;
    VkRect2D mScissor;
    VkPipelineRasterizationStateCreateInfo mRasterizer;
    VkPipelineColorBlendAttachmentState mColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo mMultisampling;
    VkPipelineLayout mPipelineLayout;
    VkPipelineDepthStencilStateCreateInfo mDepthStencil;

    VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};


class VulkanEngine {
public:
    VkInstance mInstance; // Vulkan library handle
    VkDebugUtilsMessengerEXT mDebugMessenger; // Vulkan debug output handle
    VkPhysicalDevice mChosenGPU; // GPU chosen as the default device
    VkDevice mDevice; // Vulkan device for commands
    VkSurfaceKHR mSurface; // Vulkan window surface

    VkSwapchainKHR mSwapchain; // from other articles

    // image format expected by the windowing system
    VkFormat mSwapchainImageFormat;

    //array of images from the swapchain
    std::vector<VkImage> mSwapchainImages;

    //array of image-views from the swapchain
    std::vector<VkImageView> mSwapchainImageViews;

    VkQueue mGraphicsQueue; //queue we will submit to
    uint32_t mGraphicsQueueFamily; //family of that queue

    VkCommandPool mCommandPool; //the command pool for our commands
    VkCommandBuffer mMainCommandBuffer; //the buffer we will record into

    VkRenderPass mRenderPass;

    std::vector<VkFramebuffer> mFramebuffers;

    VkSemaphore mPresentSemaphore, mRenderSemaphore;
    VkFence mRenderFence;

    VkPipelineLayout mTrianglePipelineLayout;

    VmaAllocator mAllocator; //vma lib allocator

    VkPipelineLayout mMeshPipelineLayout;
    VkPipeline mMeshPipeline;
    Mesh mTriangleMesh;

    Mesh mMonkeyMesh;

    VkImageView _depthImageView;
    VulkanType::AllocatedImage _depthImage;

    //the format for the depth image
    VkFormat _depthFormat;

    //Array of renderable objects
    std::vector<RenderObject> mRenderables;

    std::unordered_map<std::string, Material> mMaterials;
    std::unordered_map<std::string, Mesh> mMeshes;

    //Create material and it to the map
    Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

    //Returns nullptr if it can't be found
    Material* get_material(const std::string& name);

    //Returns nullptr if it can't be found
    Mesh* get_mesh(const std::string& name);

    void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);

    //initializes everything in the engine
    void init();

    //shuts down the engine
    void cleanup();

    //draw loop
    void draw();

    //run main loop
    void run();

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();

    void init_default_renderpass();

    void init_framebuffers();

    void init_sync_structures();

    void init_pipeline();

    void init_scene();

    //loads a shader module from a spir-v file. Returns false if it errors
    bool load_shader_module(const char *filePath, VkShaderModule *outShaderModule) const;

    void load_meshes();

    void upload_mesh(Mesh &mesh);

    DeletionQueue mMainDeletionQueue;

    struct SDL_Window *mWindow{nullptr};
    bool mIsInitialized{false};
    int mFrameNumber{0};

    VkExtent2D mWindowExtent{800, 600};


};
