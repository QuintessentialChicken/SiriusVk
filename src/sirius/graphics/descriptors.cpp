//
// Created by Leon on 24/09/2025.
//

#include "descriptors.h"

#include "core/types.h"
#include "fmt/chrono.h"
#include "simdjson.h"

#include <fmt/core.h>

namespace sirius {
void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type) {
    bindings_.emplace_back(VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1
    });
}

void DescriptorLayoutBuilder::Clear() {
    bindings_.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice device, const VkShaderStageFlags shaderStages, const void* pNext, VkDescriptorSetLayoutCreateFlags flags) {
    for (auto& b : bindings_) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.pNext = pNext;

    info.pBindings = bindings_.data();
    info.bindingCount = static_cast<uint32_t>(bindings_.size());
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocator::InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = static_cast<uint32_t>(ratio.ratio * maxSets)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool_);
}

void DescriptorAllocator::ClearDescriptors(VkDevice device) const {
    vkResetDescriptorPool(device, pool_, 0);
}

void DescriptorAllocator::DestroyPool(VkDevice device) const {
    vkDestroyDescriptorPool(device, pool_, nullptr);
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDevice device, VkDescriptorSetLayout layout) const {
    VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}


void DescriptorAllocatorGrowable::Init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios) {
    ratios_.clear();

    for (auto& ratio : poolRatios) {
        ratios_.push_back(ratio);
    }

    VkDescriptorPool newPool = CreatePool(device, initialSets, poolRatios);

    setsPerPool_ = static_cast<uint32_t>(initialSets * 1.5);

    readyPools_.push_back(newPool);
}

void DescriptorAllocatorGrowable::ClearPools(VkDevice device) {
    for (auto pool : readyPools_) {
        vkResetDescriptorPool(device, pool, 0);
    }

    for (auto pool : fullPools_) {
        vkResetDescriptorPool(device, pool, 0);
        readyPools_.push_back(pool);
    }

    fullPools_.clear();
}

void DescriptorAllocatorGrowable::DestroyPools(VkDevice device) {
    for (auto pool : readyPools_) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    readyPools_.clear();
    for (auto pool : fullPools_) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    fullPools_.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::Allocate(VkDevice device, VkDescriptorSetLayout layout, const void* pNext) {
    VkDescriptorPool poolToUse = GetPool(device);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = pNext;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        fullPools_.push_back(poolToUse);

        poolToUse = GetPool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }

    readyPools_.push_back(poolToUse);
    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto [type, ratio] : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = type,
            .descriptorCount = static_cast<uint32_t>(ratio * setCount)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = 0;
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool));
    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::GetPool(VkDevice device) {
    VkDescriptorPool newPool;

    if (!readyPools_.empty()) {
        newPool = readyPools_.back();
        readyPools_.pop_back();
    } else {
        newPool = CreatePool(device, setsPerPool_, ratios_);
        setsPerPool_ = static_cast<uint32_t>(setsPerPool_ * 1.5);
        if (setsPerPool_ > 4092) setsPerPool_ = 4092;
    }

    return newPool;
}

void DescriptorWriter::WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type) {
    VkDescriptorImageInfo& imageInfo = imageInfos_.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout
    });

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &imageInfo;

    writes_.push_back(write);
}

void DescriptorWriter::WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
    VkDescriptorBufferInfo& bufferInfo = bufferInfos_.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size
    });

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &bufferInfo;

    writes_.push_back(write);
}

void DescriptorWriter::Clear() {
    writes_.clear();
    imageInfos_.clear();
    bufferInfos_.clear();
}

void DescriptorWriter::UpdateSet(VkDevice device, VkDescriptorSet set) {
    for (VkWriteDescriptorSet& write : writes_) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, writes_.size(), writes_.data(), 0, nullptr);
}
} // namespace sirius
