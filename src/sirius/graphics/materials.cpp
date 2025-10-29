//
// Created by Leon on 25/10/2025.
//

#include "materials.h"

#include "initializers.h"
#include "pipelines.h"
#include <fmt/base.h>

namespace sirius {

void GltfMetallicRoughness::BuildPipelines(VkDevice device, VkFormat drawImageFormat, VkFormat depthImageFormat, VkDescriptorSetLayout sceneDataDescriptorLayout) {
    VkShaderModule fragShader;
    if (!LoadShaderModule("../../src/sirius/shaders/mesh.frag.spv", device, &fragShader)) {
        fmt::println("Error while building frag shader module");
    }
    VkShaderModule vertShader;
    if (!LoadShaderModule("../../src/sirius/shaders/mesh.vert.spv", device, &vertShader)) {
        fmt::println("Error while building vert shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GpuDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder{};
    layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialLayout_ = layoutBuilder.Build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] {sceneDataDescriptorLayout, materialLayout_};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = init::pipeline_layout_create_info();
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = layouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &matrixRange;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &newLayout));

    opaquePipeline_.layout = newLayout;
    transparentPipeline_.layout = newLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.SetShaders(vertShader, fragShader);
    pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.SetMultisamplingNone();
    pipelineBuilder.DisableBlending();
    pipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.SetColorAttachmentFormat(drawImageFormat);
    pipelineBuilder.SetDepthFormat(depthImageFormat);
    pipelineBuilder.pipelineLayout_ = newLayout;

    opaquePipeline_.pipeline = pipelineBuilder.BuildPipeline(device);

    // Transparent variant
    pipelineBuilder.EnableBlendingAdditive();
    pipelineBuilder.EnableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline_.pipeline = pipelineBuilder.BuildPipeline(device);

    vkDestroyShaderModule(device, fragShader, nullptr);
    vkDestroyShaderModule(device, vertShader, nullptr);

}

void GltfMetallicRoughness::ClearResources(VkDevice device) {
}

MaterialInstance GltfMetallicRoughness::WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator) {
    MaterialInstance materialData;
    materialData.passType = pass;
    if (pass == MaterialPass::kTransparent) {
        materialData.pipeline = &transparentPipeline_;
    } else {
        materialData.pipeline = &opaquePipeline_;
    }

    materialData.materialSet = descriptorAllocator.Allocate(device, materialLayout_);

    writer_.Clear();
    writer_.WriteBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer_.WriteImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer_.WriteImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer_.UpdateSet(device, materialData.materialSet);

    return materialData;
}
} // sirius