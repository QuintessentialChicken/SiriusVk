//
// Created by Leon on 18/09/2025.
//
#pragma once


#include "device.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace sirius {
class srsModel {
public:
    struct Vertex {
        glm::vec2 pos;
        glm::vec3 color;
        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    srsModel(srsDevice& device, const std::vector<Vertex>& vertices);
    ~srsModel();


    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);
private:
    void createVertexBuffer(const std::vector<Vertex>& vertices);

    srsDevice& device;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t vertexCount;
};
}
