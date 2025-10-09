//
// Created by Leon on 24/09/2025.
//
#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>


// TODO Handle unset pipeline info structs, maybe with optionals
namespace sirius {
class PipelineBuilder {
public:
    PipelineBuilder() { clear(); }

    VkPipeline BuildPipeline(VkDevice device);

    void clear();

    void SetShaders(VkShaderModule vertShader, VkShaderModule fragShader);

    void SetInputTopology(VkPrimitiveTopology topology);

    void SetPolygonMode(VkPolygonMode polygonMode);

    void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    void SetMultisamplingNone();

    void DisableBlending();

    void SetColorAttachmentFormat(VkFormat format);

    void SetDepthFormat(VkFormat format);

    void DisableDepthTest();

    VkPipelineLayout pipelineLayout_;
private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages_;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly_;
    VkPipelineRasterizationStateCreateInfo rasterizer_;
    VkPipelineColorBlendAttachmentState colorBlendAttachment_;
    VkPipelineMultisampleStateCreateInfo multisampling_;
    VkPipelineDepthStencilStateCreateInfo depthStencil_;
    VkPipelineRenderingCreateInfo renderInfo_;
    VkFormat colorAttachmentFormat_;
};

bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
