//
// Created by alexm on 23/12/2021.
//

#pragma once

#include "VulkanTypes.h"
#include "VulkanMesh.h"
#include "VulkanShaders.h"
#include "VulkanTools.h"

#include "ImGuiLayer.h"

#include <vector>
#include <functional>
#include <deque>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <SDL_events.h>

#include "Log.h"

struct Material {
    VkDescriptorSet textureSet{VK_NULL_HANDLE};
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

struct Texture {
    AllocatedImage image;
    VkImageView imageView;
};

struct RenderObject {
    Mesh *mesh;

    Material *material;

    glm::mat4 transformMatrix;
};

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};

struct FrameData {
    VkSemaphore mPresentSemaphore;
    VkSemaphore mRenderSemaphore;
    VkFence mRenderFence;

    VkCommandPool mCommandPool; //the command pool for our commands
    VkCommandBuffer mMainCommandBuffer; //the buffer we will record into

    //buffer that holds a single GPUCameraData to use when rendering
    AllocatedBufferUntyped cameraBuffer;
    AllocatedBufferUntyped objectBuffer;

    VkDescriptorSet globalDescriptor;
    VkDescriptorSet objectDescriptor;

};

struct UploadContext {
    VkFence mUploadFence;
    VkCommandPool mCommandPool;
    VkCommandBuffer mCommandBuffer;
};

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 renderMatrix;
};

struct GPUSceneData {
    glm::vec4 fogColour; // w is for exponent
    glm::vec4 fogDistances; //x for min, y for max, zw unused.
    glm::vec4 ambientColour;
    glm::vec4 sunlightDirection; //w for sun power
    glm::vec4 sunlightColour;
};

struct GPUObjectData {
    glm::mat4 modelMatrix;
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

//number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
    //initializes everything in the engine
    void init();

    //shuts down the engine
    void cleanup();

    //draw loop
    void draw();

    //run main loop
    void run();

    //getter for the frame we are rendering to right now.
    FrameData &get_current_frame();

    //Create material and it to the map
    Material *create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string_view &name);

    //Returns nullptr if it can't be found
    Material *get_material(const std::string &name);

    //Returns nullptr if it can't be found
    Mesh *get_mesh(const std::string &name);

    void draw_objects(VkCommandBuffer cmd, RenderObject *first, int count);

    size_t pad_uniform_buffer_size(size_t originalSize) const;

    void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);

    // This function is incomplete
    ImTextureID AddTexture(VkImageLayout imageLayout, VkImageView imageView, VkSampler sampler);

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

    VkRenderPass mRenderPass;

    std::vector<VkFramebuffer> mFramebuffers;

    //frame storage
    FrameData mFrames[FRAME_OVERLAP];

    VmaAllocator mAllocator; //vma lib allocator

    VkImageView mDepthImageView;
    AllocatedImage mDepthImage;

    //the format for the depth image
    VkFormat mDepthFormat;

    //Array of renderable objects
    std::vector<RenderObject> mRenderables;

    std::unordered_map<std::string_view, Material> mMaterials;
    std::unordered_map<std::string_view, Mesh> mMeshes;
    std::unordered_map<std::string_view, Texture> mLoadedTextures;

    AllocatedBufferUntyped create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                                         VkMemoryPropertyFlags required_flags = 0) const;

    VkDescriptorSetLayout mGlobalSetLayout;
    VkDescriptorSetLayout mObjectSetLayout;
    VkDescriptorPool mDescriptorPool;

    VkPhysicalDeviceProperties mGpuProperties;

    GPUSceneData mSceneParameters;
    AllocatedBufferUntyped mSceneParameterBuffer;

    UploadContext mUploadContext;

    VkDescriptorSetLayout mSingleTextureSetLayout;

    ImguiLayer layer;

    std::string mCurrentProjectPath;

    //-----------------------------------
    DeletionQueue mMainDeletionQueue;

    struct SDL_Window *mWindow{nullptr};
    bool mIsInitialized{false};
    int mFrameNumber{0};

    VkExtent2D mWindowExtent{1920, 1080};
    //-----------------------------------

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();

    void init_default_renderpass();

    void init_framebuffers();

    void init_sync_structures();

    void init_descriptors();

    void init_pipeline();

    void init_scene();

    void init_imgui();

    //loads a shader module from a spir-v file. Returns false if it errors
    VkShaderModule load_shader_module(const char *filePath);

    void load_meshes();

    void load_images();

    bool load_image_to_cache(const char *name, const char *path);

    void upload_mesh(Mesh &mesh);
};
