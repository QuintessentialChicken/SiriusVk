//
// Created by Leon on 18/09/2025.
//

#pragma once

#include <memory>

#include "device.h"
#include "model.h"
#include "pipeline.h"
#include "swapchain.h"

namespace sirius {
class srsVkRenderer {
public:
    srsVkRenderer();
    ~srsVkRenderer();
    void drawFrame();

private:
    void loadModels();
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();
    void recordCommandBuffers(int imageIndex);
    void recreateSwapchain();

    srsDevice device{};
    std::unique_ptr<srsSwapchain> swapchain;
    std::unique_ptr<srsPipeline> pipeline;
    VkPipelineLayout pipelineLayout;
    std::vector<VkCommandBuffer> commandBuffers;
    std::unique_ptr<srsModel> model;
};
}
