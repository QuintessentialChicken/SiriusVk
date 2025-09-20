//
// Created by Leon on 18/09/2025.
//

#pragma once

#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>


#include "device.h"
#include "model.h"
#include "pipeline.h"
#include "swapchain.h"
#include "core/object.h"


namespace sirius {

class srsVkRenderer {
public:
    srsVkRenderer();
    ~srsVkRenderer();
    void drawFrame();

private:
    void loadObjects();
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();
    void recordCommandBuffers(int imageIndex);
    void recreateSwapchain();
    void freeCommandBuffers();
    void renderObjects(VkCommandBuffer commandBuffer);

    srsDevice device{};
    std::unique_ptr<srsSwapchain> swapchain;
    std::unique_ptr<srsPipeline> pipeline;
    VkPipelineLayout pipelineLayout;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<srsObject> objects;
};
}
