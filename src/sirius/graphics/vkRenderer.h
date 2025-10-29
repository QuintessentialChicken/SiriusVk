//
// Created by Leon on 18/09/2025.
//

#pragma once


#include "types.h"
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vec4.hpp>
#include <vulkan/vulkan_core.h>
#include "descriptors.h"

#include "asset_loader.h"
#include "camera.h"
#include "materials.h"

namespace sirius {
class DeletionQueue {
public:
    std::deque<std::function<void()> > deleters_;

    void PushFunction(std::function<void()>&& function) {
        deleters_.push_back(function);
    }

    void Flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deleters_.rbegin(); it != deleters_.rend(); it++) {
            (*it)(); //call functors
        }

        deleters_.clear();
    }
};

struct FrameData {
    VkFence renderFence;
    VkSemaphore acquireSemaphore;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    DescriptorAllocatorGrowable frameDescriptors;

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

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
    std::vector<RenderObject> opaqueRenderObjects;
};

class MeshNode final : public Node {
public:
    std::shared_ptr<MeshAsset> mesh_;

    void Draw(const glm::mat4& topMatrix, DrawContext& context) override;
};

constexpr unsigned int kFrameOverlap = 3;

class SrsVkRenderer {
public:
    void Init();

    void Draw();

    void SpawnImguiWindow();

    GpuMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    void ResizeSwapChain();

    bool ResizeRequested();

    void UpdateCamera(std::pair<float, float> keyInput, std::pair<float, float> mouseInput);

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

    void CreateSwapChain(uint32_t width, uint32_t height);

    void DestroySwapChain();

    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t requestedWidth, uint32_t requestedHeight);

    void CreateImageViews();

    void InitCommandBuffers();

    void InitSyncObjects();

    void InitAllocator();

    void InitDescriptors();

    void InitPipelines();

    void InitBackgroundPipelines();

    void InitMeshPipeline();

    void InitDefaultData();

    void InitImgui();

    void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

    void DrawBackground(VkCommandBuffer cmd);

    void DrawGeometry(VkCommandBuffer cmd);

    void UpdateScene();

    AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    void DestroyBuffer(const AllocatedBuffer& buffer) const;

    AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

    AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

    void DestroyImage(const AllocatedImage& image) const;

    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    FrameData& GetCurrentFrame() { return frames_[frameNumber_ % kFrameOverlap]; }

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
    std::vector<VkSemaphore> submitSemaphores_;

    // immediate submit structures
    VkFence immFence_{};
    VkCommandBuffer immCommandBuffer_{};
    VkCommandPool immCommandPool_{};

    VmaAllocator allocator_ = nullptr;
    AllocatedImage drawImage_{};
    VkExtent2D drawExtent_{};
    AllocatedImage depthImage_{};
    DescriptorAllocatorGrowable globalDescriptorAllocator_{};
    VkDescriptorSet drawImageDescriptors_{};
    VkDescriptorSetLayout drawImageDescriptorLayout_{};
    GpuSceneData sceneData_{};
    VkDescriptorSetLayout sceneDataDescriptorLayout_{};

    VkPipeline gradientPipeline_{};
    VkPipelineLayout gradientPipelineLayout_{};
    VkPipeline meshPipeline_{};
    VkPipelineLayout meshPipelineLayout_{};

    GpuMeshBuffers rectangle_{};
    std::vector<std::shared_ptr<MeshAsset> > testMeshes_{};

    AllocatedImage whiteImage_{};
    AllocatedImage blackImage_{};
    AllocatedImage greyImage_{};
    AllocatedImage errorCheckerboardImage_{};

    VkDescriptorSetLayout singleImageDescriptorLayout_{};

    VkSampler defaultSamplerLinear_{};
    VkSampler defaultSamplerNearest_{};

    bool resizeRequested_ = false;


    bool isInitialized_ = false;

    int frameNumber_{0};
    FrameData frames_[kFrameOverlap]{};

    std::vector<ComputeEffect> computeEffects_{};
    int currentEffect_ = 0;
    float renderScale_ = 1.f;

    VkPipelineLayout trianglePipelineLayout_{};
    VkPipeline trianglePipeline_{};

    MaterialInstance defaultMaterialData_{};
    GltfMetallicRoughness metalRoughMaterial_{};

    DrawContext mainDrawContext_;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes_;

    Camera defaultCamera_{};

    DeletionQueue mainDeletionQueue_;
};
}
