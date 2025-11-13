//
// Created by Leon on 15/10/2025.
//

#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "descriptors.h"
#include "types.h"
#include "vkRenderer.h"
#include "fastgltf/types.hpp"

#undef LoadImage        // Undefine annyoing windows macro

namespace sirius {
class SrsVkRenderer;

struct GltfMaterial {
    MaterialInstance data;
};

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GltfMaterial> material;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;
    GpuMeshBuffers meshBuffers;
};

class LoadedGltf : public IRenderable {
public:
    ~LoadedGltf() override { ClearAll(); };

    void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;

    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes_;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes_;
    std::unordered_map<std::string, AllocatedImage> images_;
    std::unordered_map<std::string, std::shared_ptr<GltfMaterial>> materials_;

    // nodes that don't have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes_;

    std::vector<VkSampler> samplers_;

    DescriptorAllocatorGrowable descriptorPool_;

    AllocatedBuffer materialDataBuffer_;

    SrsVkRenderer* creator_;

private:
    void ClearAll();
};

std::optional<std::shared_ptr<LoadedGltf>> LoadGltf(SrsVkRenderer* renderer, std::string_view filePath);
std::optional<AllocatedImage> LoadImage(SrsVkRenderer* renderer, fastgltf::Asset& asset, fastgltf::Image& image);


VkFilter ExtractFilter(fastgltf::Filter filter);

VkSamplerMipmapMode ExtractMipMapMode(fastgltf::Filter filter);
}
