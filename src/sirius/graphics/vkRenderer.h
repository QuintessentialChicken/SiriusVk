//
// Created by Leon on 18/09/2025.
//

#pragma once


#include "core/types.h"
#include <deque>
#include <functional>
#include <optional>
#include <vec4.hpp>
#include <vulkan/vulkan_core.h>

#include "descriptors.h"

namespace sirius {
class deletionQueue
{
public:
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) {
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); //call functors
        }

        deletors.clear();
    }
};

struct frameData {
    VkSemaphore swapchainSemaphore, renderSemaphore;
    VkFence renderFence;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    deletionQueue deletionQueue;
};

struct computePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

constexpr unsigned int FRAME_OVERLAP = 3;


class srsVkRenderer {
public:
    void init();

    void draw();

    void drawBackground(VkCommandBuffer cmd);

    void shutdown();

private:
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME
    };

    struct swapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    struct queueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };


    bool checkValidationLayerSupport();

    void createInstance();

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    bool isDeviceSuitable(VkPhysicalDevice device);

    queueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    swapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    void createSwapChain();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void createImageViews();

    void initCommandBuffers();

    void initSyncObjects();

    void initAllocator();

    void initDescriptors();

    void initPipelines();

    void initBackgroundPipelines();

    void initImgui();

    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);


    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat = {};
    VkExtent2D swapChainExtent = {};

    VmaAllocator allocator;
    AllocatedImage drawImage;
    VkExtent2D drawExtent;

    descriptorAllocator globalDescriptorAllocator;
    VkDescriptorSet drawImageDescriptors;
    VkDescriptorSetLayout drawImageDescriptorLayout;

    VkPipeline gradientPipeline;
    VkPipelineLayout gradientPipelineLayout;

    // immediate submit structures
    VkFence immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;


    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    bool isInitialized = false;

    int frameNumber {0};
    frameData frames[FRAME_OVERLAP] {};
    frameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }

    deletionQueue mainDeletionQueue;

};
}
