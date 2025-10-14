//
// Created by Leon on 22/09/2025.
//

#pragma once

#include <fwd.hpp>
#include <vec3.hpp>
#include <vec4.hpp>
#include <mat4x4.hpp>

#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

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


// uv coords interleaved for alignement
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

#define VK_CHECK(x)                                                     \
do {                                                                \
VkResult err = x;                                               \
if (err) {                                                      \
fmt::print("Detected Vulkan error: {}", string_VkResult(err)); \
abort();                                                    \
}                                                               \
} while (0)
