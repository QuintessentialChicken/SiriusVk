//
// Created by Leon on 24/09/2025.
//
#pragma once
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace sirius {
class DescriptorLayoutBuilder {
public:
    std::vector<VkDescriptorSetLayoutBinding> bindings_;

    void AddBinding(uint32_t binding, VkDescriptorType type);

    void Clear();

    VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

class DescriptorAllocator {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool_;

    void InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);

    void ClearDescriptors(VkDevice device);

    void DestroyPool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
}
