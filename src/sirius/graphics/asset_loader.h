//
// Created by Leon on 15/10/2025.
//

#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/types.h"

namespace sirius {
class SrsVkRenderer;
}

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;
    GpuMeshBuffers meshBuffers;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(sirius::SrsVkRenderer* engine, std::filesystem::path filePath);


