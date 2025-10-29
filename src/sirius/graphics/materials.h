//
// Created by Leon on 25/10/2025.
//
#pragma once

#include "descriptors.h"
#include "types.h"

namespace sirius {

class GltfMetallicRoughness {
public:
    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metalRoughFactors;
        // Padding to align at 256 bytes
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    MaterialPipeline opaquePipeline_{};
    MaterialPipeline transparentPipeline_{};

    VkDescriptorSetLayout materialLayout_{};

    DescriptorWriter writer_;

    void BuildPipelines(VkDevice device, VkFormat drawImageFormat, VkFormat depthImageFormat, VkDescriptorSetLayout sceneDataDescriptorLayout);
    void ClearResources(VkDevice device);
    MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};
} // sirius
