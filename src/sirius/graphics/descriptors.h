//
// Created by Leon on 24/09/2025.
//
#pragma once
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace sirius {
class descriptorLayoutBuilder {
public:
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(uint32_t binding, VkDescriptorType type);

    void clear();

    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

class descriptorAllocator {
public:
    struct poolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void initPool(VkDevice device, uint32_t maxSets, std::span<poolSizeRatio> poolRatios);

    void clearDescriptors(VkDevice device);

    void destroyPool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
}
