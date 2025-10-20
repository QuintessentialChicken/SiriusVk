//
// Created by Leon on 24/09/2025.
//
#pragma once
#include <deque>
#include <span>
#include <string>
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

class DescriptorAllocatorGrowable {
public:

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    void Init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void ClearPools(VkDevice device);
    void DestroyPools(VkDevice device);

    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
    VkDescriptorPool CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);
    VkDescriptorPool GetPool(VkDevice device);

    std::vector<PoolSizeRatio> ratios_;
    std::vector<VkDescriptorPool> fullPools_;
    std::vector<VkDescriptorPool> readyPools_;
    uint32_t setsPerPool_{1000};

};

class DescriptorWriter {
public:
    void WriteImage(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
    void WriteBuffer(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type);

    void Clear();
    void UpdateSet(VkDevice device, VkDescriptorSet set);

private:
    std::deque<VkDescriptorImageInfo> imageInfos_;
    std::deque<VkDescriptorBufferInfo> bufferInfos_;
    std::vector<VkWriteDescriptorSet> writes_;
};

}
