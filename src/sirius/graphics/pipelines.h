//
// Created by Leon on 24/09/2025.
//
#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace sirius {
class pipelineBuilder {
    pipelineBuilder() { clear(); }

    VkPipeline buildPipeline(VkDevice device);

    void clear();

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineLayout pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineRenderingCreateInfo renderInfo;
    VkFormat colorAttachmentformat;
};

bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
