//
// Created by alexm on 23/12/2021.
//

#pragma once

#include "VulkanTypes.h"
#include "VulkanTools.h"
#include "VulkanMesh.h"
#include "VulkanScene.h"
#include "VulkanShaders.h"
#include "VulkanPushBuffer.h"

#include "VulkanCamera.h"
#include "MaterialSystem.h"

#include "ImGuiLayer.h"

#include <vector>
#include <functional>
#include <deque>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <SDL_events.h>
#include "FrustumCull.h"

namespace vkutil { struct Material; }

namespace tracy { class VkCtx; }

namespace assets { struct PrefabInfo; }

//forward declarations
namespace vkutil {
    class DescriptorLayoutCache;
    class DescriptorAllocator;
    class VulkanProfiler;
}

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

namespace assets {
    enum class TransparencyMode :uint8_t;
}

struct Material {
    VkDescriptorSet textureSet{VK_NULL_HANDLE};
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

struct Texture {
    AllocatedImage image;
    VkImageView imageView;
};

struct MeshObject {
    Mesh* mesh{ nullptr };

    vkutil::Material* material;
    uint32_t customSortKey;
    glm::mat4 transformMatrix;

    RenderBounds bounds;

    uint32_t bDrawForwardPass : 1;
    uint32_t bDrawShadowPass : 1;
};

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};

struct FrameData {
    VkSemaphore _presentSemaphore, _renderSemaphore;
    VkFence renderFence;

    DeletionQueue _frameDeletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    vkutil::PushBuffer dynamicData;

    AllocatedBufferUntyped debugOutputBuffer;

    vkutil::DescriptorAllocator* dynamicDescriptorAllocator;

    std::vector<uint32_t> debugDataOffsets;
    std::vector<std::string> debugDataNames;
};

struct UploadContext {
    VkFence mUploadFence;
    VkCommandPool mCommandPool;
};

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 renderMatrix;
};

struct GPUSceneData {
    glm::vec4 fogColor; // w is for exponent
    glm::vec4 fogDistances; //x for min, y for max, zw unused.
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; //w for sun power
    glm::vec4 sunlightColor;
    glm::mat4 sunlightShadowMatrix;
};

struct GPUObjectData {
    glm::mat4 modelMatrix;
    glm::vec4 origin_rad; // bounds
    glm::vec4 extents;  // bounds
};

struct EngineStats {
    float frametime;
    int objects;
    int drawcalls;
    int draws;
    int triangles;
};

struct MeshDrawCommands {
    struct RenderBatch {
        MeshObject* object;
        uint64_t sortKey;
        uint64_t objectIndex;
    };

    std::vector<RenderBatch> batch;
};

struct /*alignas(16)*/DrawCullData
{
    glm::mat4 viewMat;
    float P00, P11, znear, zfar; // symmetric projection parameters
    float frustum[4]; // data for left/right/top/bottom frustum planes
    float lodBase, lodStep; // lod distance i = base * pow(step, i)
    float pyramidWidth, pyramidHeight; // depth pyramid size in texels

    uint32_t drawCount;

    int cullingEnabled;
    int lodEnabled;
    int occlusionEnabled;
    int distanceCheck;
    int AABBcheck;
    float aabbmin_x;
    float aabbmin_y;
    float aabbmin_z;
    float aabbmax_x;
    float aabbmax_y;
    float aabbmax_z;
};

struct DirectionalLight {
    glm::vec3 lightPosition;
    glm::vec3 lightDirection;
    glm::vec3 shadowExtent;
    glm::mat4 get_projection();

    glm::mat4 get_view();
};

struct CullParams {
    glm::mat4 viewmat;
    glm::mat4 projmat;
    bool occlusionCull;
    bool frustrumCull;
    float drawDist;
    bool aabb;
    glm::vec3 aabbmin;
    glm::vec3 aabbmax;
};

//number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP = 2;
const int MAX_OBJECTS = 150000;

class VulkanEngine {
public:
    bool mIsInitialized{ false };
    int mFrameNumber {0};
    int mSelectedShader{ 0 };

    VkExtent2D mWindowExtent{ 1700 , 900 };

    struct SDL_Window* mWindow{ nullptr };

    VkInstance mInstance;
    VkPhysicalDevice mChosenGPU;
    VkDevice mDevice;

    VkPhysicalDeviceProperties mGpuProperties;

    FrameData mFrames[FRAME_OVERLAP];

    VkQueue mGraphicsQueue;
    uint32_t mGraphicsQueueFamily;

    tracy::VkCtx* mGraphicsQueueContext;

    VkRenderPass mRenderPass;
    VkRenderPass mShadowPass;
    VkRenderPass mCopyPass;

    VkSurfaceKHR mSurface;
    VkSwapchainKHR mSwapchain;
    VkFormat mSwapchainImageFormat;

    VkFormat mRenderFormat;
    AllocatedImage mRawRenderImage;
    VkSampler mSmoothSampler;
    VkFramebuffer mForwardFramebuffer;
    VkFramebuffer mShadowFramebuffer;
    std::vector<VkFramebuffer> mFramebuffers;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    DeletionQueue mMainDeletionQueue;

    VmaAllocator mAllocator; //vma lib allocator

    //depth resources

    AllocatedImage mDepthImage;
    AllocatedImage mDepthPyramid;
    VkSampler mShadowSampler;
    AllocatedImage mShadowImage;
    //VkExtent2D _shadowExtent{1024,1024};
    VkExtent2D mShadowExtent{ 1024*4,1024*4 };
    int depthPyramidWidth ;
    int depthPyramidHeight;
    int depthPyramidLevels;

    //the format for the depth image
    VkFormat mDepthFormat;

    vkutil::DescriptorAllocator* mDescriptorAllocator;
    vkutil::DescriptorLayoutCache* mDescriptorLayoutCache;
    vkutil::VulkanProfiler* mProfiler;
    vkutil::MaterialSystem* mMaterialSystem;

    VkDescriptorSetLayout mSingleTextureSetLayout;

    GPUSceneData mSceneParameters;

    std::vector<VkBufferMemoryBarrier> uploadBarriers;

    std::vector<VkBufferMemoryBarrier> cullReadyBarriers;

    std::vector<VkBufferMemoryBarrier> postCullBarriers;

    UploadContext mUploadContext;

    PlayerCamera mCamera;
    DirectionalLight mMainLight;

    VkPipeline mCullPipeline;
    VkPipelineLayout mCullLayout;

    VkPipeline mDepthReducePipeline;
    VkPipelineLayout mDepthReduceLayout;

    VkPipeline mSparseUploadPipeline;
    VkPipelineLayout mSparseUploadLayout;

    VkPipeline mBlitPipeline;
    VkPipelineLayout mBlitLayout;

    VkSampler mDepthSampler;
    VkImageView mDepthPyramidMips[16] = {};

    MeshDrawCommands mCurrentCommands;
    RenderScene mRenderScene;

    ImguiLayer layer;

    //EngineConfig _config;

    void ready_mesh_draw(VkCommandBuffer cmd);

    //initializes everything in the engine
    void init();

    //shuts down the engine
    void cleanup();

    //draw loop
    void draw();

    void forward_pass(VkClearValue clearValue, VkCommandBuffer cmd);

    void shadow_pass(VkCommandBuffer cmd);


    //run main loop
    void run();

    FrameData& get_current_frame();
    FrameData& get_last_frame();

    ShaderCache mShaderCache;

    std::unordered_map<std::string, Mesh> mMeshes;
    std::unordered_map<std::string, Texture> mLoadedTextures;
    std::unordered_map<std::string, assets::PrefabInfo*> mPrefabCache;
    //functions

    //returns nullptr if it cant be found
    Mesh* get_mesh(const std::string& name);

    //our draw function
    void draw_objects_forward(VkCommandBuffer cmd, RenderScene::MeshPass& pass);

    void execute_draw_commands(VkCommandBuffer cmd, RenderScene::MeshPass& pass, VkDescriptorSet ObjectDataSet, std::vector<uint32_t> dynamic_offsets, VkDescriptorSet GlobalSet);

    void draw_objects_shadow(VkCommandBuffer cmd, RenderScene::MeshPass& pass);

    void reduce_depth(VkCommandBuffer cmd);

    void execute_compute_cull(VkCommandBuffer cmd, RenderScene::MeshPass& pass,CullParams& params);

    void ready_cull_data(RenderScene::MeshPass& pass, VkCommandBuffer cmd);

    AllocatedBufferUntyped
    create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags required_flags = 0);

    void reallocate_buffer(AllocatedBufferUntyped&buffer,size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags required_flags = 0);


    size_t pad_uniform_buffer_size(size_t originalSize);

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    bool load_prefab(const char* path, glm::mat4 root);

    static std::string asset_path(std::string_view path);

    static std::string shader_path(std::string_view path);
    void refresh_renderbounds(MeshObject* object);

    template<typename T>
    T* map_buffer(AllocatedBuffer<T> &buffer);

    void unmap_buffer(AllocatedBufferUntyped& buffer);

    bool load_compute_shader(const char* shaderPath, VkPipeline& pipeline, VkPipelineLayout& layout);
private:
    EngineStats stats;
    void process_input_event(SDL_Event* ev);

    void init_vulkan();

    void init_swapchain();

    void init_forward_renderpass();

    void init_copy_renderpass();

    void init_shadow_renderpass();

    void init_framebuffers();

    void init_commands();

    void init_sync_structures();

    void init_pipelines();

    void init_scene();

    void init_descriptors();

    void init_imgui();

    void load_meshes();

    void load_images();

    bool load_image_to_cache(const char* name, const char* path);

    void upload_mesh(Mesh& mesh);

    void copy_render_to_swapchain(uint32_t swapchainImageIndex, VkCommandBuffer cmd);
};

template<typename T>
T* VulkanEngine::map_buffer(AllocatedBuffer<T>& buffer)
{
    void* data;
    vmaMapMemory(mAllocator, buffer.mAllocation, &data);
    return(T*)data;
}