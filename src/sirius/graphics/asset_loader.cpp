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

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset newMesh;

        newMesh.name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& primitive : mesh.primitives) {
            GeoSurface newSurface{};
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

            size_t initialVtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](const std::uint32_t idx) {
                        indices.push_back(idx + initialVtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](const glm::vec3 v, const size_t index) {
                        Vertex newVertex{};
                        newVertex.position = v;
                        newVertex.normal = { 1, 0, 0 };
                        newVertex.color = glm::vec4 { 1.f };
                        newVertex.uvX = 0;
                        newVertex.uvY = 0;
                        vertices[initialVtx + index] = newVertex;
                    });
            }

            // load vertex normals
            if (auto normals = primitive.findAttribute("NORMAL"); normals != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
                    [&](const glm::vec3 v, const size_t index) {
                        vertices[initialVtx + index].normal = v;
                    });
            }

            // load UVs
            if (auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
                    [&](const glm::vec2 v, const size_t index) {
                        vertices[initialVtx + index].uvX = v.x;
                        vertices[initialVtx + index].uvY = v.y;
                    });
            }

            // load vertex colors
            if (auto colors = primitive.findAttribute("COLOR_0"); colors != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
                    [&](const glm::vec4 v, const size_t index) {
                        vertices[initialVtx + index].color = v;
                    });
            }
            newMesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool overrideColors = true;
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
