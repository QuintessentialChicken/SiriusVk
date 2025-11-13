//
// Created by Leon on 18/09/2025.
//

#include "vkRenderer.h"

#include <ostream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <set>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <array>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>


#include "window/wndProc.h"
#include <vulkan/vulkan_win32.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"

#include "pipelines.h"
#include "core/utils.h"
#include "initializers.h"

#include <fmt/core.h>

#include "materials.h"
#include "fastgltf/types.hpp"


#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

namespace sirius {
struct SimplePushConstantData {
    glm::mat2 transform{1.0f};
    glm::vec2 offset;
    alignas(16) glm::vec3 color;
};

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& context) {
    glm::mat4 nodeMatrix{topMatrix * worldTransform_};

    for (auto& [startIndex, count, material] : mesh_->surfaces) {
        RenderObject object{};
        object.indexCount = count;
        object.firstIndex = startIndex;
        object.indexBuffer = mesh_->meshBuffers.indexBuffer.buffer;
        object.material = &material->data;
        object.transform = nodeMatrix;
        object.vertexBufferAddress = mesh_->meshBuffers.vertexBufferAddress;

        context.opaqueRenderObjects.push_back(object);
    }

    Node::Draw(topMatrix, context);
}

void SrsVkRenderer::Init() {
    assert(hwndMain != nullptr && "Can't init renderer without window");
    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    InitAllocator();
    CreateSwapChain(windowWidth, windowHeight);
    CreateImageViews();
    InitCommandBuffers();
    InitSyncObjects();
    InitDescriptors();
    InitPipelines();
    InitImgui();
    InitDefaultData();
    defaultCamera_.Init();

    defaultCamera_.velocity_ = glm::vec3(0.0f);
    defaultCamera_.position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    defaultCamera_.pitch_ = 0.0f;
    defaultCamera_.yaw_ = 0.0f;

    isInitialized_ = true;
    std::cout << "Renderer initialized successfully \n" << "------------ Running ------------" << std::endl;
}

void SrsVkRenderer::Draw() {
    UpdateScene();

    VK_CHECK(vkWaitForFences(device_, 1, &GetCurrentFrame().renderFence, true, 1000000000));

    GetCurrentFrame().deletionQueue.Flush();
    GetCurrentFrame().frameDescriptors.ClearPools(device_);

    uint32_t imageIndex;

    VkResult e = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, GetCurrentFrame().acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        resizeRequested_ = true;
        return;
    }

    VK_CHECK(vkResetFences(device_, 1, &GetCurrentFrame().renderFence));

    VkCommandBuffer cmd = GetCurrentFrame().mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.pNext = nullptr;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // transition our main draw image into general layout so we can write into it.
    // we will overwrite it all so we don't care about what the older layout was
    Utils::TransitionFlags beforeComputeFlags{
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
    };

    Utils::TransitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, beforeComputeFlags);

    DrawBackground(cmd);

    Utils::TransitionFlags beforeGeoDrawFlags{
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    };

    Utils::TransitionFlags depthBeforeGeoDrawFlags{
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    Utils::TransitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, beforeGeoDrawFlags);
    Utils::TransitionImage(cmd, depthImage_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depthBeforeGeoDrawFlags);

    DrawGeometry(cmd);

    //transition the draw image and the swapchain image into their correct transfer layouts
    Utils::TransitionFlags beforeTransferFlagsDraw{
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
    };

    Utils::TransitionFlags beforeTransferFlagsSwapChain{
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    };

    Utils::TransitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, beforeTransferFlagsDraw);
    Utils::TransitionImage(cmd, swapChainImages_[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, beforeTransferFlagsSwapChain);

    // execute a copy from the draw image into the swapchain
    Utils::CopyImageToImage(cmd, drawImage_.image, swapChainImages_[imageIndex], drawExtent_, swapChainExtent_);

    Utils::TransitionFlags beforeImguiFlags{
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    };

    Utils::TransitionImage(cmd, swapChainImages_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, beforeImguiFlags);

    DrawImgui(cmd, swapChainImageViews_[imageIndex]);

    Utils::TransitionFlags beforePresentFlags{
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = 0,
    };
    // set swapchain image layout to Present so we can show it on the screen
    Utils::TransitionImage(cmd, swapChainImages_[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, beforePresentFlags);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdBufferInfo{};
    cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdBufferInfo.pNext = nullptr;
    cmdBufferInfo.deviceMask = 0;
    cmdBufferInfo.commandBuffer = cmd;

    VkSemaphoreSubmitInfo waitSemaphoreInfo{};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.pNext = nullptr;
    waitSemaphoreInfo.semaphore = GetCurrentFrame().acquireSemaphore;
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    waitSemaphoreInfo.deviceIndex = 0;
    waitSemaphoreInfo.value = 1;

    VkSemaphoreSubmitInfo signalSemaphoreInfo{};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.pNext = nullptr;
    signalSemaphoreInfo.semaphore = submitSemaphores_[imageIndex];
    signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    signalSemaphoreInfo.deviceIndex = 0;
    signalSemaphoreInfo.value = 1;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.pNext = nullptr;

    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;

    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdBufferInfo;


    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submitInfo, GetCurrentFrame().renderFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pImageIndices = nullptr;

    presentInfo.pSwapchains = &swapChain_;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &submitSemaphores_[imageIndex];
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(graphicsQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        resizeRequested_ = true;
        return;
    }
    //increase the number of frames drawn
    frameNumber_++;
}

void SrsVkRenderer::DrawBackground(VkCommandBuffer cmd) {
    const ComputeEffect& effect = computeEffects_.at(currentEffect_);

    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout_, 0, 1, &drawImageDescriptors_, 0, nullptr);

    vkCmdPushConstants(cmd, gradientPipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(drawExtent_.width / 16.0), std::ceil(drawExtent_.height / 16.0), 1);
}

void SrsVkRenderer::DrawGeometry(VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo colorAttachment = init::attachment_info(drawImage_.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = init::depth_attachment_info(depthImage_.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    const VkRenderingInfo renderingInfo = init::rendering_info(drawExtent_, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline_);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(drawExtent_.width);
    viewport.height = static_cast<float>(drawExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent.width = drawExtent_.width;
    scissor.extent.height = drawExtent_.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    AllocatedBuffer sceneDataBuffer{CreateBuffer(sizeof(GpuSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)};

    GetCurrentFrame().deletionQueue.PushFunction([this, sceneDataBuffer] { DestroyBuffer(sceneDataBuffer); });
    auto* sceneUniformData{static_cast<GpuSceneData*>(sceneDataBuffer.allocation->GetMappedData())};
    *sceneUniformData = sceneData_;

    VkDescriptorSet globalDescriptor{GetCurrentFrame().frameDescriptors.Allocate(device_, sceneDataDescriptorLayout_)};

    DescriptorWriter writer;
    writer.WriteBuffer(0, sceneDataBuffer.buffer, sizeof(GpuSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.UpdateSet(device_, globalDescriptor);

    for (const auto& [indexCount, firstIndex, indexBuffer, material, transform, vertexBufferAddress] : mainDrawContext_.opaqueRenderObjects) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline->layout, 1, 1, &material->materialSet, 0, nullptr);

        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        GpuDrawPushConstants pushConstants;
        pushConstants.vertexBuffer = vertexBufferAddress;
        pushConstants.worldMatrix = transform;
        vkCmdPushConstants(cmd, material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GpuDrawPushConstants), &pushConstants);

        vkCmdDrawIndexed(cmd, indexCount, 1, firstIndex, 0, 0);
    }

    vkCmdEndRendering(cmd);
}

void SrsVkRenderer::UpdateScene() {
    drawExtent_.width = drawImage_.imageExtent.width;
    drawExtent_.height = drawImage_.imageExtent.height;

    defaultCamera_.Update();

    mainDrawContext_.opaqueRenderObjects.clear();

    // loadedNodes_.at("Suzanne")->Draw(glm::mat4{1.0f}, mainDrawContext_);
    // for (int x = -3; x < 4; x++) {
    //     for (int y = -3; y < 4; y++) {
    //         for (int z = -3; z < 4; z++) {
    //             if (x == 0 && y == 0 && z == 0) {continue;}
    //             glm::mat4 scale = glm::scale(glm::vec3{0.2});
    //             glm::mat4 translation = glm::translate(glm::vec3{x * 3, y * 3, z * 3});
    //
    //             loadedNodes_["Cube"]->Draw(translation * scale, mainDrawContext_);
    //         }
    //     }
    //
    // }

    sceneData_.viewMatrix = defaultCamera_.GetViewMatrix();
    sceneData_.projectionMatrix = glm::perspectiveRH_ZO(glm::radians(70.0f), static_cast<float>(drawExtent_.width) / static_cast<float>(drawExtent_.height), 10000.0f, 0.1f);
    sceneData_.projectionMatrix[1][1] *= -1;
    sceneData_.viewProjectionMatrix = sceneData_.projectionMatrix * sceneData_.viewMatrix;

    sceneData_.ambientColor = glm::vec4(0.1f);
    sceneData_.sunlightColor = glm::vec4(1.0f);
    sceneData_.sunlightDirection = glm::vec4(0, 1, 0.5f, 1.0f);

    loadedScenes_.at("structure")->Draw(glm::mat4{ 1.0f }, mainDrawContext_);
}

AllocatedBuffer SrsVkRenderer::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocationCreateInfo = {};
    vmaAllocationCreateInfo.usage = memoryUsage;
    vmaAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer{};

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(allocator_, &bufferInfo, &vmaAllocationCreateInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

void SrsVkRenderer::DestroyBuffer(const AllocatedBuffer& buffer) const {
    vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
}

AllocatedImage SrsVkRenderer::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped) {
    AllocatedImage newImage{};
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo imageInfo = init::image_create_info(format, usage, size);
    if (mipmapped) {
        imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2((std::max)(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK(vmaCreateImage(allocator_, &imageInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo viewCreateInfo = init::imageview_create_info(format, newImage.image, aspectFlags);
    viewCreateInfo.subresourceRange.levelCount = imageInfo.mipLevels;

    VK_CHECK(vkCreateImageView(device_, &viewCreateInfo, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage SrsVkRenderer::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped) {
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadBuffer.info.pMappedData, data, dataSize);

    const AllocatedImage newImage = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        constexpr Utils::TransitionFlags beforeTransferFlags{
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        };
        Utils::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, beforeTransferFlags);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        Utils::TransitionFlags afterTransferFlags{
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, // TODO What if the image is used somewhere else?
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        };
        Utils::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, afterTransferFlags);
    });

    DestroyBuffer(uploadBuffer);

    return newImage;
}

void SrsVkRenderer::DestroyImage(const AllocatedImage& image) const {
    vkDestroyImageView(device_, image.imageView, nullptr);
    vmaDestroyImage(allocator_, image.image, image.allocation);
}

GpuMeshBuffers SrsVkRenderer::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GpuMeshBuffers newBuffer{};

    newBuffer.vertexBuffer = CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    VkBufferDeviceAddressInfo deviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};

    deviceAddressInfo.buffer = newBuffer.vertexBuffer.buffer;
    newBuffer.vertexBufferAddress = vkGetBufferDeviceAddress(device_, &deviceAddressInfo);

    newBuffer.indexBuffer = CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // Staging buffer to load data on and copy it to the GPU_ONLY buffer
    AllocatedBuffer staging = CreateBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newBuffer.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newBuffer.indexBuffer.buffer, 1, &indexCopy);
    });

    DestroyBuffer(staging);

    return newBuffer;
}

void SrsVkRenderer::SpawnImguiWindow() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();

    if (ImGui::Begin("Settings")) {
        ImGui::Text("Press Escape to use the mouse, Right-click to capture mouse");
        float framerate = ImGui::GetIO().Framerate;
        ImGui::Text("Framerate: %f", framerate);

        ComputeEffect& selected = computeEffects_[currentEffect_];

        ImGui::Text("Selected effect: ", selected.name);

        ImGui::SliderInt("Effect Index", &currentEffect_, 0, static_cast<int>(computeEffects_.size()) - 1);

        ImGui::InputFloat4("data1", reinterpret_cast<float*>(&selected.data.data1));
        ImGui::InputFloat4("data2", reinterpret_cast<float*>(&selected.data.data2));
        ImGui::InputFloat4("data3", reinterpret_cast<float*>(&selected.data.data3));
        ImGui::InputFloat4("data4", reinterpret_cast<float*>(&selected.data.data4));
    }
    ImGui::End();

    ImGui::Render();
}

void SrsVkRenderer::Shutdown() {
    if (isInitialized_) {
        vkDeviceWaitIdle(device_);

        loadedScenes_.clear();

        for (auto& frame : frames_) {
            vkDestroyCommandPool(device_, frame.commandPool, nullptr);

            vkDestroyFence(device_, frame.renderFence, nullptr);
            vkDestroySemaphore(device_, frame.acquireSemaphore, nullptr);

            frame.deletionQueue.Flush();
        }

        for (const auto semaphore : submitSemaphores_) {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }

        mainDeletionQueue_.Flush();
    }
}

bool SrsVkRenderer::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers_) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void SrsVkRenderer::CreateInstance() {
    if (kEnableValidationLayers && !CheckValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    createInfo.enabledExtensionCount = requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    if (kEnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
        createInfo.ppEnabledLayerNames = validationLayers_.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
    std::cout << "Vulkan: Instance created\n" << std::endl;
}

void SrsVkRenderer::CreateSurface() {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = hwndMain;
    createInfo.hinstance = hInstance;
    if (vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
    std::cout << "Vulkan: Surface created\n" << std::endl;
}

void SrsVkRenderer::PickPhysicalDevice() {
    uint32_t deviceCount = 0;

    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    // Check each device and pick the first that fits our criteria
    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

void SrsVkRenderer::CreateLogicalDevice() {
    QueueFamilyIndices indices = FindQueueFamilies(physicalDevice_);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }


    // Check if needed features are available

    VkPhysicalDeviceVulkan13Features supported13{};
    supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceVulkan12Features supported12{};
    supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceFeatures2 supportedFeatures{};
    supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supportedFeatures.pNext = &supported12;
    supported12.pNext = &supported13;

    vkGetPhysicalDeviceFeatures2(physicalDevice_, &supportedFeatures);

    // Create needed feature structs
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;

    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{};
    shaderObjectFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
    shaderObjectFeatures.shaderObject = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &shaderObjectFeatures;
    shaderObjectFeatures.pNext = &features13;
    features13.pNext = &features12;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions_.size());;
    createInfo.ppEnabledExtensionNames = deviceExtensions_.data();


    if (kEnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
        createInfo.ppEnabledLayerNames = validationLayers_.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }
    std::cout << "Vulkan: Logical Device created\n" << std::endl;

    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

bool SrsVkRenderer::IsDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = FindQueueFamilies(device);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.IsComplete() && extensionsSupported && swapChainAdequate;
}

SrsVkRenderer::QueueFamilyIndices SrsVkRenderer::FindQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.IsComplete()) {
            break;
        }
        i++;
    }
    return indices;
}

bool SrsVkRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions_.begin(), deviceExtensions_.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SrsVkRenderer::SwapChainSupportDetails SrsVkRenderer::QuerySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void SrsVkRenderer::CreateSwapChain(uint32_t width, uint32_t height) {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice_);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, width, height);

    // Request one more image than minimum to avoid having to potentially wait for the driver to
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // Make sure to not exceed the max image count. 0 is a special case that indicates that there's no maximum
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(physicalDevice_);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // Don't rotate images in the swapchain
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Set to opaque to ignore alpha bit. Allows blending with other windows in the window system.
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; // Ignore pixels that are obscured (like by another window)
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }
    std::cout << "Vulkan: Swapchain created\n" << std::endl;

    // Retrieve handles to the swapchain images
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
    swapChainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());

    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = extent;

    VkExtent3D drawImageExtent = {
        width,
        height,
        1
    };

    //hardcoding the draw format to 32-bit float
    drawImage_.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    drawImage_.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo drawImageInfo{};
    drawImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    drawImageInfo.pNext = nullptr;
    drawImageInfo.imageType = VK_IMAGE_TYPE_2D;
    drawImageInfo.format = drawImage_.imageFormat;
    drawImageInfo.extent = drawImageExtent;
    drawImageInfo.mipLevels = 1;
    drawImageInfo.arrayLayers = 1;
    //for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
    drawImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    //optimal tiling, which means the image is stored on the best gpu format
    drawImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    drawImageInfo.usage = drawImageUsages;

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo drawImageAllocInfo = {};
    drawImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    drawImageAllocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(allocator_, &drawImageInfo, &drawImageAllocInfo, &drawImage_.image, &drawImage_.allocation, nullptr);

    depthImage_.imageFormat = VK_FORMAT_D32_SFLOAT;
    depthImage_.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsage{};
    depthImageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo depthImageInfo = init::image_create_info(depthImage_.imageFormat, depthImageUsage, drawImageExtent);
    vmaCreateImage(allocator_, &depthImageInfo, &drawImageAllocInfo, &depthImage_.image, &depthImage_.allocation, nullptr);

    VkImageViewCreateInfo depthImageViewInfo = init::imageview_create_info(depthImage_.imageFormat, depthImage_.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    vkCreateImageView(device_, &depthImageViewInfo, nullptr, &depthImage_.imageView);

    //build an image view for the draw image to use for rendering
    VkImageViewCreateInfo renderViewInfo{};
    renderViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    renderViewInfo.pNext = nullptr;
    renderViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    renderViewInfo.image = drawImage_.image;
    renderViewInfo.format = drawImage_.imageFormat;
    renderViewInfo.subresourceRange.baseMipLevel = 0;
    renderViewInfo.subresourceRange.levelCount = 1;
    renderViewInfo.subresourceRange.baseArrayLayer = 0;
    renderViewInfo.subresourceRange.layerCount = 1;
    renderViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VK_CHECK(vkCreateImageView(device_, &renderViewInfo, nullptr, &drawImage_.imageView));

    //add to deletion queues
    mainDeletionQueue_.PushFunction([this]() {
        vkDestroyImageView(device_, drawImage_.imageView, nullptr);
        vmaDestroyImage(allocator_, drawImage_.image, drawImage_.allocation);

        vkDestroyImageView(device_, depthImage_.imageView, nullptr);
        vmaDestroyImage(allocator_, depthImage_.image, depthImage_.allocation);
    });
}

void SrsVkRenderer::ResizeSwapChain() {
    vkDeviceWaitIdle(device_);
    DestroySwapChain();

    CreateSwapChain(windowWidth, windowHeight);
    CreateImageViews();
    resizeRequested_ = false;
}

bool SrsVkRenderer::ResizeRequested() {
    return resizeRequested_;
}

void SrsVkRenderer::DestroySwapChain() {
    vkDestroySwapchainKHR(device_, swapChain_, nullptr);

    for (const auto& view : swapChainImageViews_) {
        vkDestroyImageView(device_, view, nullptr);
    }

    for (const auto& image : swapChainImages_) {
        vkDestroyImage(device_, image, nullptr);
    }

    vkDestroyImage(device_, drawImage_.image, nullptr);
    vkDestroyImage(device_, depthImage_.image, nullptr);
}

VkSurfaceFormatKHR SrsVkRenderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR SrsVkRenderer::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (auto presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SrsVkRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t requestedWidth, uint32_t requestedHeight) {
    // If the currentExtent is set to the maximum value of uint32_t it means that the extent of the swapchain determines the surface size
    if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
        return capabilities.currentExtent;
    }
    VkExtent2D actualExtent = {
        requestedWidth,
        requestedHeight
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

void SrsVkRenderer::CreateImageViews() {
    swapChainImageViews_.clear();
    swapChainImageViews_.resize(swapChainImages_.size());
    for (size_t i = 0; i < swapChainImages_.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Allows for remapping of the channels (like mapping all channels to one for a monochrome texture)
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &createInfo, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view " + std::to_string(i));
        }
    }
}

void SrsVkRenderer::InitCommandBuffers() {
    QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice_);
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    for (auto& frame : frames_) {
        VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &frame.commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext = nullptr;
        cmdAllocInfo.commandPool = frame.commandPool;
        cmdAllocInfo.commandBufferCount = 1;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &frame.mainCommandBuffer));
    }

    VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &immCommandPool_));

    // allocate the command buffer for immediate submits

    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = nullptr;
    cmdAllocInfo.commandPool = immCommandPool_;
    cmdAllocInfo.commandBufferCount = 1;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &immCommandBuffer_));

    mainDeletionQueue_.PushFunction([this]() {
        vkDestroyCommandPool(device_, immCommandPool_, nullptr);
    });
}

void SrsVkRenderer::InitSyncObjects() {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (auto& frame : frames_) {
        VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &frame.renderFence));

        VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.acquireSemaphore));
    }

    submitSemaphores_.resize(swapChainImages_.size());
    for (int i = 0; i < swapChainImages_.size(); i++) {
        VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &submitSemaphores_[i]));
    }

    VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &immFence_));
    mainDeletionQueue_.PushFunction([this]() { vkDestroyFence(device_, immFence_, nullptr); });
}

void SrsVkRenderer::InitAllocator() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice_;
    allocatorInfo.device = device_;
    allocatorInfo.instance = instance_;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator_);

    mainDeletionQueue_.PushFunction([&]() {
        vmaDestroyAllocator(allocator_);
    });
}

void SrsVkRenderer::InitDescriptors() {
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };

    globalDescriptorAllocator_.Init(device_, 10, sizes);

    //make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawImageDescriptorLayout_ = builder.Build(device_, VK_SHADER_STAGE_COMPUTE_BIT);
    } {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        sceneDataDescriptorLayout_ = builder.Build(device_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    } {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        singleImageDescriptorLayout_ = builder.Build(device_, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    drawImageDescriptors_ = globalDescriptorAllocator_.Allocate(device_, drawImageDescriptorLayout_);

    DescriptorWriter writer;
    writer.WriteImage(0, drawImage_.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(device_, drawImageDescriptors_);

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    mainDeletionQueue_.PushFunction([&]() {
        globalDescriptorAllocator_.DestroyPools(device_);

        vkDestroyDescriptorSetLayout(device_, drawImageDescriptorLayout_, nullptr);
        vkDestroyDescriptorSetLayout(device_, sceneDataDescriptorLayout_, nullptr);
        vkDestroyDescriptorSetLayout(device_, singleImageDescriptorLayout_, nullptr);
    });

    for (auto& frame : frames_) {
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4}
        };

        frame.frameDescriptors = DescriptorAllocatorGrowable{};
        frame.frameDescriptors.Init(device_, 1000, frameSizes);

        mainDeletionQueue_.PushFunction([&] {
            frame.frameDescriptors.DestroyPools(device_);
        });
    }
}

void SrsVkRenderer::InitPipelines() {
    InitBackgroundPipelines();
    InitMeshPipeline();
    metalRoughMaterial_.BuildPipelines(device_, drawImage_.imageFormat, depthImage_.imageFormat, sceneDataDescriptorLayout_);
}

void SrsVkRenderer::InitBackgroundPipelines() {
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &drawImageDescriptorLayout_;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pushConstantRangeCount = 1;
    computeLayout.pPushConstantRanges = &pushConstant;

    VK_CHECK(vkCreatePipelineLayout(device_, &computeLayout, nullptr, &gradientPipelineLayout_));

    //layout code
    VkShaderModule gradientShader;
    if (!LoadShaderModule("../../src/sirius/shaders/gradient_color.comp.spv", device_, &gradientShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    VkShaderModule skyShader;
    if (!LoadShaderModule("../../src/sirius/shaders/sky.comp.spv", device_, &skyShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = gradientShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = gradientPipelineLayout_;
    computePipelineCreateInfo.stage = stageInfo;

    ComputeEffect gradient{};
    gradient.layout = gradientPipelineLayout_;
    gradient.name = "gradient";
    gradient.data = {
        glm::vec4(1, 0, 0, 1),
        glm::vec4(0, 0, 1, 1)
    };

    VK_CHECK(vkCreateComputePipelines(device_,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &gradient.pipeline));

    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect sky{};
    sky.layout = gradientPipelineLayout_;
    sky.name = "sky";
    sky.data = {
        glm::vec4(0.1f, 0.2f, 0.4f, 0.97f)
    };

    VK_CHECK(vkCreateComputePipelines(device_,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &sky.pipeline));

    computeEffects_.push_back(gradient);
    computeEffects_.push_back(sky);

    vkDestroyShaderModule(device_, gradientShader, nullptr);
    vkDestroyShaderModule(device_, skyShader, nullptr);

    mainDeletionQueue_.PushFunction([&]() {
        vkDestroyPipelineLayout(device_, gradientPipelineLayout_, nullptr);
        vkDestroyPipeline(device_, gradient.pipeline, nullptr);
        vkDestroyPipeline(device_, sky.pipeline, nullptr);
    });
}

void SrsVkRenderer::InitMeshPipeline() {
    VkShaderModule fragShader;
    if (!LoadShaderModule("../../src/sirius/shaders/textured_image.frag.spv", device_, &fragShader)) {
        fmt::print("Error when building the triangle fragment shader module\n");
    } else {
        fmt::print("Triangle fragment shader successfully loaded\n");
    }

    VkShaderModule triangleVertexShader;
    if (!LoadShaderModule("../../src/sirius/shaders/colored_triangle_mesh.vert.spv", device_, &triangleVertexShader)) {
        fmt::print("Error when building the triangle vertex shader module\n");
    } else {
        fmt::print("Triangle vertex shader successfully loaded\n");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GpuDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = init::pipeline_layout_create_info();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &singleImageDescriptorLayout_;

    VK_CHECK(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &meshPipelineLayout_));

    PipelineBuilder pipelineBuilder;

    //use the triangle layout we created
    pipelineBuilder.pipelineLayout_ = meshPipelineLayout_;
    //connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.SetShaders(triangleVertexShader, fragShader);
    //it will draw triangles
    pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //filled triangles
    pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    //no backface culling
    pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    //no multisampling
    pipelineBuilder.SetMultisamplingNone();

    pipelineBuilder.DisableBlending();

    pipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //connect the image format we will draw into, from draw image
    pipelineBuilder.SetColorAttachmentFormat(drawImage_.imageFormat);
    pipelineBuilder.SetDepthFormat(depthImage_.imageFormat);

    //finally build the pipeline
    meshPipeline_ = pipelineBuilder.BuildPipeline(device_);

    //clean structures
    vkDestroyShaderModule(device_, fragShader, nullptr);
    vkDestroyShaderModule(device_, triangleVertexShader, nullptr);

    mainDeletionQueue_.PushFunction([&]() {
        vkDestroyPipelineLayout(device_, meshPipelineLayout_, nullptr);
        vkDestroyPipeline(device_, meshPipeline_, nullptr);
    });
}

void SrsVkRenderer::InitDefaultData() {
    std::array<Vertex, 4> rectVertices{};

    rectVertices[0].position = {0.5, -0.5, 0};
    rectVertices[1].position = {0.5, 0.5, 0};
    rectVertices[2].position = {-0.5, -0.5, 0};
    rectVertices[3].position = {-0.5, 0.5, 0};

    rectVertices[0].color = {1, 0, 0, 1};
    rectVertices[1].color = {0, 0, 0, 0.9};
    rectVertices[2].color = {0, 0, 0, 0.9};
    rectVertices[3].color = {0, 0, 0, 0.9};

    std::array<uint32_t, 6> rectIndices{};

    rectIndices[0] = 0;
    rectIndices[1] = 1;
    rectIndices[2] = 2;

    rectIndices[3] = 2;
    rectIndices[4] = 1;
    rectIndices[5] = 3;

    rectangle_ = UploadMesh(rectIndices, rectVertices);

    //delete the rectangle data on engine shutdown
    mainDeletionQueue_.PushFunction([&]() {
        DestroyBuffer(rectangle_.indexBuffer);
        DestroyBuffer(rectangle_.vertexBuffer);
    });

    //3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    whiteImage_ = CreateImage((void*) &white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    greyImage_ = CreateImage((void*) &grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    blackImage_ = CreateImage((void*) &black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // checkerboard image
    const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels{}; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    errorCheckerboardImage_ = CreateImage(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo samplerCreateInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(device_, &samplerCreateInfo, nullptr, &defaultSamplerNearest_);

    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device_, &samplerCreateInfo, nullptr, &defaultSamplerLinear_);

    mainDeletionQueue_.PushFunction([&] {
        vkDestroySampler(device_, defaultSamplerNearest_, nullptr);
        vkDestroySampler(device_, defaultSamplerLinear_, nullptr);

        DestroyImage(whiteImage_);
        DestroyImage(greyImage_);
        DestroyImage(blackImage_);
        DestroyImage(errorCheckerboardImage_);
    });

    GltfMetallicRoughness::MaterialResources materialResources{};
    //default the material textures
    materialResources.colorImage = whiteImage_;
    materialResources.colorSampler = defaultSamplerLinear_;
    materialResources.metalRoughImage = whiteImage_;
    materialResources.metalRoughSampler = defaultSamplerLinear_;

    //set the uniform buffer for the material data
    AllocatedBuffer materialConstants = CreateBuffer(sizeof(GltfMetallicRoughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //write the buffer
    auto* sceneUniformData = static_cast<GltfMetallicRoughness::MaterialConstants*>(materialConstants.allocation->GetMappedData());
    sceneUniformData->colorFactors = glm::vec4{1, 1, 1, 1};
    sceneUniformData->metalRoughFactors = glm::vec4{1, 0.5, 0, 0};

    mainDeletionQueue_.PushFunction([=, this]() {
        DestroyBuffer(materialConstants);
    });

    materialResources.dataBuffer = materialConstants.buffer;
    materialResources.dataBufferOffset = 0;

    defaultMaterialData_ = metalRoughMaterial_.WriteMaterial(device_, MaterialPass::kMainColor, materialResources, globalDescriptorAllocator_);

    std::string structurePath = { "../../resources/structure.glb" };
    auto structureFile = LoadGltf(this, structurePath);
    assert(structureFile.has_value());
    loadedScenes_["structure"] = *structureFile;
}

void SrsVkRenderer::InitImgui() {
    IMGUI_CHECKVERSION();

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &imguiPool));

    ImGui::CreateContext();

    ImGui_ImplWin32_Init(hwndMain);

    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance = instance_;
    info.PhysicalDevice = physicalDevice_;
    info.Device = device_;
    info.Queue = graphicsQueue_;
    info.DescriptorPool = imguiPool;
    info.MinImageCount = 3;
    info.ImageCount = 3;
    info.UseDynamicRendering = true;

    info.PipelineInfoMain.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainImageFormat_;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;


    ImGui_ImplVulkan_Init(&info);

    mainDeletionQueue_.PushFunction([&]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(device_, imguiPool, nullptr);
    });
}

void SrsVkRenderer::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = init::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = init::rendering_info(swapChainExtent_, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}


// Record commands to a Command Buffer, submit them immediately to the GPU and wait for it to be finished with them
void SrsVkRenderer::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(device_, 1, &immFence_));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer_, 0));

    VkCommandBuffer cmd = immCommandBuffer_;

    VkCommandBufferBeginInfo cmdBeginInfo = init::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = init::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = init::submit_info(&cmdInfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, immFence_));

    VK_CHECK(vkWaitForFences(device_, 1, &immFence_, true, 9999999999));
}
}
