//
// Created by Leon on 15/10/2025.
//

#include "asset_loader.h"

#include <iostream>
#include <ext/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>

#include "vkRenderer.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "fmt/compile.h"

namespace sirius {
void LoadedGltf::Draw(const glm::mat4& topMatrix, DrawContext& ctx) {
    for (auto& node : topNodes_) {
        node->Draw(topMatrix, ctx);
    }
}

void LoadedGltf::ClearAll() {
}

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(sirius::SrsVkRenderer* engine, std::filesystem::path filePath) {
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

    std::vector<std::shared_ptr<MeshAsset>> meshes;

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

std::optional<std::shared_ptr<LoadedGltf>> LoadGltf(SrsVkRenderer* renderer, std::string_view filePath) {
    fmt::print("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGltf> scene = std::make_shared<LoadedGltf>();
    scene->creator_ = renderer;
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
            std::cerr << "Failed to load glb: " << fastgltf::to_underlying(load.error()) << std::endl;
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

    file.descriptorPool_.Init(renderer->device_, gltf.materials.size(), sizes);

    for (fastgltf::Sampler& sampler : gltf.samplers) {
        VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = ExtractMipMapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(renderer->device_, &sampl, nullptr, &newSampler);

        file.samplers_.push_back(newSampler);
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GltfMaterial>> materials;

    for (fastgltf::Image& image : gltf.images) {
        images.push_back(renderer->errorCheckerboardImage_);
    }
    file.materialDataBuffer_ = renderer->CreateBuffer(sizeof(GltfMetallicRoughness::MaterialConstants) * gltf.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int dataIndex = 0;
    auto sceneMaterialConstants = static_cast<GltfMetallicRoughness::MaterialConstants*>(file.materialDataBuffer_.info.pMappedData);

    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GltfMaterial> newMat = std::make_shared<GltfMaterial>();
        materials.push_back(newMat);
        file.materials_[mat.name.c_str()] = newMat;

        GltfMetallicRoughness::MaterialConstants constants{};
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metalRoughFactors.x = mat.pbrData.metallicFactor;
        constants.metalRoughFactors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        sceneMaterialConstants[dataIndex] = constants;

        MaterialPass passType = MaterialPass::kMainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::kTransparent;
        }

        GltfMetallicRoughness::MaterialResources materialResources{};
        // default the material textures
        materialResources.colorImage = renderer->whiteImage_;
        materialResources.colorSampler = renderer->defaultSamplerLinear_;
        materialResources.metalRoughImage = renderer->whiteImage_;
        materialResources.metalRoughSampler = renderer->defaultSamplerLinear_;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer_.buffer;
        materialResources.dataBufferOffset = dataIndex * sizeof(GltfMetallicRoughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers_[sampler];
        }
        // build material
        newMat->data = renderer->metalRoughMaterial_.WriteMaterial(renderer->device_, passType, materialResources, file.descriptorPool_);

        dataIndex++;
    }

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes_[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& primitive : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

            size_t initialVtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                                                         [&](std::uint32_t idx) {
                                                             indices.push_back(idx + initialVtx);
                                                         });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                              [&](glm::vec3 v, size_t index) {
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
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex], [&](glm::vec3 v, size_t index) {
                    vertices[initialVtx + index].normal = v;
                });
            }

            // load UVs
            auto uv = primitive.findAttribute("TEXCOORD_0");
            if (uv != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
                                                              [&](glm::vec2 v, size_t index) {
                                                                  vertices[initialVtx + index].uvX = v.x;
                                                                  vertices[initialVtx + index].uvY = v.y;
                                                              });
            }

            // load vertex colors
            auto colors = primitive.findAttribute("COLOR_0");
            if (colors != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
                                                              [&](glm::vec4 v, size_t index) {
                                                                  vertices[initialVtx + index].color = v;
                                                              });
            }

            if (primitive.materialIndex.has_value()) {
                newSurface.material = materials[primitive.materialIndex.value()];
            } else {
                newSurface.material = materials[0];
            }

            newmesh->surfaces.push_back(newSurface);
        }

        newmesh->meshBuffers = renderer->UploadMesh(indices, vertices);
    }

    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the MeshNode class
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh_ = meshes[*node.meshIndex];
        } else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes_[node.name.c_str()];

        std::visit(
            fastgltf::visitor{
                [&](const fastgltf::math::fmat4x4& matrix) {
                    memcpy(&newNode->localTransform_, matrix.data(), sizeof(matrix));
                },
                [&](fastgltf::TRS transform) {
                    const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                    const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                    const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                    const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                    const glm::mat4 rm = glm::toMat4(rot);
                    const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                    newNode->localTransform_ = tm * rm * sm;
                }
            }, node.transform);
    }

    // Set up hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children) {
            sceneNode->children_.push_back(nodes[c]);
            nodes[c]->parent_ = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes) {
        if (node->parent_.lock() == nullptr) {
            file.topNodes_.push_back(node);
            node->RefreshTransform(glm::mat4 { 1.f });
        }
    }
    return scene;
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
