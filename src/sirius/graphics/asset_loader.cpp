//
// Created by Leon on 15/10/2025.
//

#include "asset_loader.h"

#include <iostream>

#include "vkRenderer.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "fmt/compile.h"

namespace sirius {
void LoadedGltf::Draw(const glm::mat4& topMatrix, DrawContext& ctx) {
}

void LoadedGltf::ClearAll() {
}

std::optional<std::vector<std::shared_ptr<MeshAsset> > > LoadGltfMeshes(sirius::SrsVkRenderer* engine, std::filesystem::path filePath) {
    std::cout << "\n" << "Loading GLTF: " << filePath << "\n" << std::endl;

    fastgltf::Expected<fastgltf::GltfDataBuffer> result = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (!result) {
        throw std::runtime_error("Failed to load glTF: " + std::to_string(static_cast<int>(result.error())));
    }
    auto data = std::move(result.get());

    constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadGltfBinary(data, filePath.parent_path(), gltfOptions);
    if (load) {
        gltf = std::move(load.get());
    } else {
        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }

    std::vector<std::shared_ptr<MeshAsset> > meshes;

    // use the same vectors for all meshes so that the memory doesn't reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset newMesh;

        newMesh.name = mesh.name;

        // clear the mesh arrays each mesh, we don't want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& primitive : mesh.primitives) {
            GeoSurface newSurface{};
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

            size_t initialVtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](const std::uint32_t idx) {
                    indices.push_back(idx + initialVtx);
                });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](const glm::vec3 v, const size_t index) {
                    Vertex newVertex{};
                    newVertex.position = v;
                    newVertex.normal = {1, 0, 0};
                    newVertex.color = glm::vec4{1.f};
                    newVertex.uvX = 0;
                    newVertex.uvY = 0;
                    vertices[initialVtx + index] = newVertex;
                });
            }

            // load vertex normals
            if (auto normals = primitive.findAttribute("NORMAL"); normals != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex], [&](const glm::vec3 v, const size_t index) {
                    vertices[initialVtx + index].normal = v;
                });
            }

            // load UVs
            if (auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex], [&](const glm::vec2 v, const size_t index) {
                    vertices[initialVtx + index].uvX = v.x;
                    vertices[initialVtx + index].uvY = v.y;
                });
            }

            // load vertex colors
            if (auto colors = primitive.findAttribute("COLOR_0"); colors != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex], [&](const glm::vec4 v, const size_t index) {
                    vertices[initialVtx + index].color = v;
                });
            }
            newMesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool overrideColors = false;
        if (overrideColors) {
            for (Vertex& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }
        newMesh.meshBuffers = engine->UploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    return meshes;
}

std::optional<std::shared_ptr<LoadedGltf> > LoadGltf(SrsVkRenderer* engine, std::string_view filePath) {
    fmt::print("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGltf> scene = std::make_shared<LoadedGltf>();
    scene->creator_ = engine;
    LoadedGltf& file = *scene;

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    fastgltf::Expected<fastgltf::GltfDataBuffer> result = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (!result) {
        throw std::runtime_error("Failed to load glTF: " + std::to_string(static_cast<int>(result.error())));
    }
    auto data = std::move(result.get());

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;

    auto type = fastgltf::determineGltfFileType(data);
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGltfBinary(data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadGltfBinary(data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    } else {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    };

    file.descriptorPool_.Init(engine->device_, gltf.materials.size(), sizes);

    for (fastgltf::Sampler& sampler : gltf.samplers) {

        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = ExtractMipMapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->device_, &sampl, nullptr, &newSampler);

        file.samplers_.push_back(newSampler);
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GltfMaterial>> materials;

    for (fastgltf::Image& image : gltf.images) {
        images.push_back(engine->errorCheckerboardImage_);
    }

}

VkFilter ExtractFilter(fastgltf::Filter filter) {
    switch (filter) {
        // nearest samplers
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;

        // linear samplers
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode ExtractMipMapMode(fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}
}
