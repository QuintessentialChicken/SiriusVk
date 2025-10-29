//
// Created by Leon on 22/09/2025.
//

#pragma once

#include <fwd.hpp>
#include <vec3.hpp>
#include <vec4.hpp>
#include <mat4x4.hpp>
#include <memory>
#include <vector>

#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

namespace sirius {
struct DrawContext;

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};


// uv coords interleaved for alignment
struct Vertex {
    glm::vec3 position;
    float uvX;
    glm::vec3 normal;
    float uvY;
    glm::vec4 color;
};

struct GpuMeshBuffers {
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GpuDrawPushConstants {
    glm::mat4 worldMatrix{};
    VkDeviceAddress vertexBuffer{};
};

struct GpuSceneData {
    glm::mat4 viewMatrix{};
    glm::mat4 projectionMatrix{};
    glm::mat4 viewProjectionMatrix{};
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
};

enum class MaterialPass : uint8_t {
    kMainColor,
    kTransparent,
    kOther
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

class IRenderable {
public:
    virtual ~IRenderable() = default;

private:
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& context) = 0;
};

class Node : public IRenderable {
public:

    std::weak_ptr<Node> parent_;
    std::vector<std::shared_ptr<Node>> children_;

    glm::mat4 localTransform_{};
    glm::mat4 worldTransform_{};

    void RefreshTransform(const glm::mat4& parentMatrix) {
        worldTransform_ = parentMatrix * localTransform_;
        for (const auto& child : children_) {
            child->RefreshTransform(worldTransform_);
        }
    }

    void Draw(const glm::mat4& topMatrix, DrawContext& context) override {
        for (const auto& child : children_) {
            child->Draw(topMatrix, context);
        }
    }
};


#define VK_CHECK(x)                                                     \
do {                                                                \
VkResult err = x;                                               \
if (err) {                                                      \
fmt::print("Detected Vulkan error: {}", string_VkResult(err)); \
abort();                                                    \
}                                                               \
} while (0)
}
