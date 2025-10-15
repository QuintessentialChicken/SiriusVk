//
// Created by Leon on 18/09/2025.
//

#pragma once


#include "core/types.h"
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vec4.hpp>
#include <vulkan/vulkan_core.h>

#include "descriptors.h"

struct MeshAsset;

namespace sirius {
class DeletionQueue
{
public:
    std::deque<std::function<void()>> deletors_;

    void PushFunction(std::function<void()>&& function) {
        deletors_.push_back(function);
    }

    void Flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors_.rbegin(); it != deletors_.rend(); it++) {
            (*it)(); //call functors
        }

        deletors_.clear();
    }
};

struct FrameData {
    VkSemaphore swapchainSemaphore, renderSemaphore;
    VkFence renderFence;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    DeletionQueue deletionQueue;
};

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

constexpr unsigned int kFrameOverlap = 3;


class SrsVkRenderer {
public:
    void Init();

    void Draw();

    void SpawnImguiWindow();

    GpuMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    void Shutdown();

private:
    const std::vector<const char*> deviceExtensions_ = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    const std::vector<const char*> validationLayers_ = {
        "VK_LAYER_KHRONOS_validation"
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]] bool IsComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };


    bool CheckValidationLayerSupport();

    void CreateInstance();

    void CreateSurface();

    void PickPhysicalDevice();

    void CreateLogicalDevice();

    bool IsDeviceSuitable(VkPhysicalDevice device);

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

    void CreateSwapChain();

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void CreateImageViews();

    void InitCommandBuffers();

    void InitSyncObjects();

    void InitAllocator();

    void InitDescriptors();

    void InitPipelines();

    void InitBackgroundPipelines();

    void InitTrianglePipeline();

    void InitMeshPipeline();

    void InitDefaultData();

    void InitImgui();

    void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

    void DrawBackground(VkCommandBuffer cmd);

    void DrawGeometry(VkCommandBuffer cmd);

    AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    void DestroyBuffer(const AllocatedBuffer& buffer) const;


    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;
    VkFormat swapChainImageFormat_ = {};
    VkExtent2D swapChainExtent_ = {};

    VmaAllocator allocator_ = nullptr;
    AllocatedImage drawImage_;
    VkExtent2D drawExtent_;

    DescriptorAllocator globalDescriptorAllocator_;
    VkDescriptorSet drawImageDescriptors_;
    VkDescriptorSetLayout drawImageDescriptorLayout_;

    VkPipeline gradientPipeline_;
    VkPipelineLayout gradientPipelineLayout_;

    VkPipeline meshPipeline_;
    VkPipelineLayout meshPipelineLayout_;

	GpuMeshBuffers rectangle;
    std::vector<std::shared_ptr<MeshAsset>> testMeshes_;


    // immediate submit structures
    VkFence immFence_;
    VkCommandBuffer immCommandBuffer_;
    VkCommandPool immCommandPool_;


    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    bool isInitialized_ = false;

    int frameNumber_ {0};
    FrameData frames_[kFrameOverlap] {};
    FrameData& GetCurrentFrame() { return frames_[frameNumber_ % kFrameOverlap]; }

    std::vector<ComputeEffect> computeEffects_;
    int currentEffect_ = 0;
	float renderScale_ = 1.f;

    VkPipelineLayout trianglePipelineLayout_;
    VkPipeline trianglePipeline_;


    DeletionQueue mainDeletionQueue_;

};
}
