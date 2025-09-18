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
class srsRenderer {
public:
    srsRenderer();
    ~srsRenderer();
    void drawFrame();

private:
    void loadModels();
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();

    srsDevice device{};
    srsSwapchain swapchain{device, {1280, 720}};
    std::unique_ptr<srsPipeline> pipeline;
    VkPipelineLayout pipelineLayout;
    std::vector<VkCommandBuffer> commandBuffers;
    std::unique_ptr<srsModel> model;
};
}
