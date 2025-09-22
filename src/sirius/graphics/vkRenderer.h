//
// Created by Leon on 18/09/2025.
//

#pragma once

#include <deque>
#include <functional>
#include <optional>
#include <vulkan/vulkan_core.h>
namespace sirius {
class DeletionQueue
{
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

struct FrameData {
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    DeletionQueue _deletionQueue;
};

constexpr unsigned int FRAME_OVERLAP = 2;


class srsVkRenderer {
public:
    void init();

    void shutdown();

private:
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
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

    int frameNumber {0};
    FrameData frames[FRAME_OVERLAP];
    FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }


};
}
