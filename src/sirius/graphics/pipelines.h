//
// Created by Leon on 24/09/2025.
//
#pragma once
#include <vulkan/vulkan_core.h>

namespace sirius {
class pipelines {
};

bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
