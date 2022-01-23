#pragma once

#include <VulkanTypes.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <material_asset.h>

#include <VulkanMesh.h>

class PipelineBuilder {
public:

    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    VertexInputDescription vertexDescription;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkViewport _viewport;
    VkRect2D _scissor;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
    void clear_vertex_input();

    void setShaders(struct ShaderEffect* effect);
};

enum class VertexAttributeTemplate {
    DefaultVertex,
    DefaultVertexPosOnly
};

class EffectBuilder {

    VertexAttributeTemplate vertexAttrib;
    struct ShaderEffect* effect{ nullptr };

    VkPrimitiveTopology topology;
    VkPipelineRasterizationStateCreateInfo rasterizerInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
};

class ComputePipelineBuilder {
public:

    VkPipelineShaderStageCreateInfo  _shaderStage;
    VkPipelineLayout _pipelineLayout;
    VkPipeline build_pipeline(VkDevice device);
};
struct ShaderEffect;
class VulkanEngine;
namespace vkutil {

    struct ShaderPass {
        ShaderEffect* effect{ nullptr };
        VkPipeline pipeline{ VK_NULL_HANDLE };
        VkPipelineLayout layout{ VK_NULL_HANDLE };
    };

    struct SampledTexture {
        VkSampler sampler;
        VkImageView view;
    };
    struct ShaderParameters
    {

    };

    template<typename T>
    struct PerPassData {

    public:
        T& operator[](MeshpassType pass)
        {
            switch (pass)
            {
                case MeshpassType::Forward:
                    return data[0];
                case MeshpassType::Transparency:
                    return data[1];
                case MeshpassType::DirectionalShadow:
                    return data[2];
            }
            assert(false);
            return data[0];
        };

        void clear(T&& val)
        {
            for (int i = 0; i < 3; i++)
            {
                data[i] = val;
            }
        }

    private:
        std::array<T, 3> data;
    };


    struct EffectTemplate {
        PerPassData<ShaderPass*> passShaders;

        ShaderParameters* defaultParameters;
        assets::TransparencyMode transparency;
    };

    struct MaterialData {
        std::vector<SampledTexture> textures;
        ShaderParameters* parameters;
        std::string baseTemplate;

        bool operator==(const MaterialData& other) const;

        size_t hash() const;
    };

    struct Material {
        EffectTemplate* original;
        PerPassData<VkDescriptorSet> passSets;

        std::vector<SampledTexture> textures;

        ShaderParameters* parameters;

        Material& operator=(const Material& other) = default;
    };

    class MaterialSystem {
    public:
        void init(VulkanEngine* owner);
        void cleanup();

        void build_default_templates();

        ShaderPass* build_shader(VkRenderPass renderPass,PipelineBuilder& builder, ShaderEffect* effect);

        Material* build_material(const std::string& materialName, const MaterialData& info);
        Material* get_material(const std::string& materialName);

        void fill_builders();
    private:

        struct MaterialInfoHash
        {

            std::size_t operator()(const MaterialData& k) const
            {
                return k.hash();
            }
        };

        PipelineBuilder forwardBuilder;
        PipelineBuilder shadowBuilder;


        std::unordered_map<std::string, EffectTemplate> templateCache;
        std::unordered_map<std::string, Material*> materials;
        std::unordered_map<MaterialData, Material*, MaterialInfoHash> materialCache;
        VulkanEngine* engine;
    };
}
