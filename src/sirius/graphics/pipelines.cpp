//
// Created by Leon on 24/09/2025.
//

#include "pipelines.h"

#include <fstream>
#include <iosfwd>
#include <vector>

#include "fmt/base.h"
#include "initializers.h"

VkPipeline sirius::PipelineBuilder::BuildPipeline(VkDevice device) {
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.pNext = nullptr;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment_;

    VkPipelineVertexInputStateCreateInfo vertexInputState{};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderInfo_;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages_.size());
    pipelineInfo.pStages = shaderStages_.data();
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssembly_;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer_;
    pipelineInfo.pMultisampleState = &multisampling_;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDepthStencilState = &depthStencil_;
    pipelineInfo.layout = pipelineLayout_;

    VkDynamicState state[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
    };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        fmt::println("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    return newPipeline;
}

void sirius::PipelineBuilder::clear() {
    inputAssembly_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };

    rasterizer_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };

    colorBlendAttachment_ = {};

    multisampling_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };

    pipelineLayout_ = {};

    depthStencil_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    renderInfo_ = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

    shaderStages_.clear();
}

void sirius::PipelineBuilder::SetShaders(VkShaderModule vertShader, VkShaderModule fragShader) {
    shaderStages_.clear();
    shaderStages_.emplace_back(init::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertShader));
    shaderStages_.emplace_back(init::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader));
}

void sirius::PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology) {
    inputAssembly_.topology = topology;
    inputAssembly_.primitiveRestartEnable = VK_FALSE;
}

void sirius::PipelineBuilder::SetPolygonMode(VkPolygonMode polygonMode) {
    rasterizer_.polygonMode = polygonMode;
    rasterizer_.lineWidth = 1.0f;
}

void sirius::PipelineBuilder::SetCullMode(VkCullModeFlags cullMode,
                                          VkFrontFace frontFace) {
    rasterizer_.cullMode = cullMode;
    rasterizer_.frontFace = frontFace;
}

void sirius::PipelineBuilder::SetMultisamplingNone() {
    multisampling_.sampleShadingEnable = VK_FALSE;
    multisampling_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling_.minSampleShading = 1.0f;
    multisampling_.pSampleMask = nullptr;
    multisampling_.alphaToCoverageEnable = VK_FALSE;
    multisampling_.alphaToOneEnable = VK_FALSE;
}

void sirius::PipelineBuilder::DisableBlending() {
    colorBlendAttachment_.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment_.blendEnable = VK_FALSE;
}

void sirius::PipelineBuilder::SetColorAttachmentFormat(VkFormat format) {
    colorAttachmentFormat_ = format;
    renderInfo_.colorAttachmentCount = 1;
    renderInfo_.pColorAttachmentFormats = &colorAttachmentFormat_;
}

void sirius::PipelineBuilder::SetDepthFormat(VkFormat format) {
    renderInfo_.depthAttachmentFormat = format;
}

void sirius::PipelineBuilder::DisableDepthTest() {
    depthStencil_.depthTestEnable = VK_FALSE;
    depthStencil_.depthWriteEnable = VK_FALSE;
    depthStencil_.depthCompareOp = VK_COMPARE_OP_NEVER;
    depthStencil_.depthBoundsTestEnable = VK_FALSE;
    depthStencil_.stencilTestEnable = VK_FALSE;
    depthStencil_.front = {};
    depthStencil_.back = {};
    depthStencil_.minDepthBounds = 0.f;
    depthStencil_.maxDepthBounds = 1.f;
}

bool sirius::load_shader_module(const char* filePath, VkDevice device,
                                VkShaderModule* outShaderModule) {
    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t) file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
        VK_SUCCESS) {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}
