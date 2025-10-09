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
#include <glm/glm.hpp>

#include "window/wndProc.h"
#include <vulkan/vulkan_win32.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"

#include "pipelines.h"
#include "core/utils.h"
#include "initializers.h"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

namespace sirius {
struct SimplePushConstantData {
    glm::mat2 transform{1.0f};
    glm::vec2 offset;
    alignas(16) glm::vec3 color;
};

void SrsVkRenderer::Init() {
    assert(hwndMain != nullptr && "Can't init renderer without window");
    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    InitAllocator();
    CreateSwapChain();
    CreateImageViews();
    InitCommandBuffers();
    InitSyncObjects();
    InitDescriptors();
    InitPipelines();
    InitImgui();
    isInitialized_ = true;
}

void SrsVkRenderer::Draw() {
    VK_CHECK(vkWaitForFences(device_, 1, &GetCurrentFrame().renderFence, true, 1000000000));

    GetCurrentFrame().deletionQueue.Flush();

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, GetCurrentFrame().swapchainSemaphore, VK_NULL_HANDLE, &imageIndex);

    VK_CHECK(vkResetFences(device_, 1, &GetCurrentFrame().renderFence));

    VkCommandBuffer cmd = GetCurrentFrame().mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));


    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.pNext = nullptr;


    drawExtent_.width = drawImage_.imageExtent.width;
    drawExtent_.height = drawImage_.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));


    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    Utils::TransitionFlags beforeComputeFlags{
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
    };

    Utils::TransitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, beforeComputeFlags);


    DrawBackground(cmd);


    //transition the draw image and the swapchain image into their correct transfer layouts
    Utils::TransitionFlags beforeTransferFlagsDraw{
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
    };

    Utils::TransitionFlags beforeTransferFlagsSwapChain{
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    };


    Utils::TransitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, beforeTransferFlagsDraw);
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
    waitSemaphoreInfo.semaphore = GetCurrentFrame().swapchainSemaphore;
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    waitSemaphoreInfo.deviceIndex = 0;
    waitSemaphoreInfo.value = 1;

    VkSemaphoreSubmitInfo signalSemaphoreInfo{};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.pNext = nullptr;
    signalSemaphoreInfo.semaphore = GetCurrentFrame().renderSemaphore;
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

    presentInfo.swapchainCount = 0;
    presentInfo.pSwapchains = nullptr;
    presentInfo.pWaitSemaphores = nullptr;
    presentInfo.waitSemaphoreCount = 0;
    presentInfo.pImageIndices = nullptr;

    presentInfo.pSwapchains = &swapChain_;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(graphicsQueue_, &presentInfo);


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

void SrsVkRenderer::SpawnImguiWindow() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();

    if (ImGui::Begin("background")) {

        ImGui::SliderFloat("Render Scale",&renderScale_, 0.3f, 1.f);

        ComputeEffect& selected = computeEffects_[currentEffect_];

        ImGui::Text("Selected effect: ", selected.name);

        ImGui::SliderInt("Effect Index", &currentEffect_,0, computeEffects_.size() - 1);

        ImGui::InputFloat4("data1",(float*)& selected.data.data1);
        ImGui::InputFloat4("data2",(float*)& selected.data.data2);
        ImGui::InputFloat4("data3",(float*)& selected.data.data3);
        ImGui::InputFloat4("data4",(float*)& selected.data.data4);

        ImGui::End();
    }

    ImGui::Render();
}

void SrsVkRenderer::Shutdown() {
    if (isInitialized_) {
        vkDeviceWaitIdle(device_);

        for (auto& frame : frames_) {
            vkDestroyCommandPool(device_, frame.commandPool, nullptr);

            vkDestroyFence(device_, frame.renderFence, nullptr);
            vkDestroySemaphore(device_, frame.renderSemaphore, nullptr);
            vkDestroySemaphore(device_, frame.swapchainSemaphore, nullptr);

            frame.deletionQueue.Flush();
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
    if (enableValidationLayers && !CheckValidationLayerSupport()) {
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

    if (enableValidationLayers) {
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

    VkPhysicalDeviceFeatures2 supported_features{};
    supported_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported_features.pNext = &supported12;
    supported12.pNext = &supported13;

    vkGetPhysicalDeviceFeatures2(physicalDevice_, &supported_features);

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


    if (enableValidationLayers) {
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

void SrsVkRenderer::CreateSwapChain() {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice_);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

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
        windowHeight,
        windowHeight,
        1
    };

    //hardcoding the draw format to 32 bit float
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

    //build a image-view for the draw image to use for rendering
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
    });
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

VkExtent2D SrsVkRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // If the currentExtent is set to the maximum value of uint32_t it means that the extend of the swapchain determines the surface size
    if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
        return capabilities.currentExtent;
    }
    VkExtent2D actualExtent = {
        static_cast<uint32_t>(windowWidth),
        static_cast<uint32_t>(windowHeight)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

void SrsVkRenderer::CreateImageViews() {
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

    for (int i = 0; i < kFrameOverlap; i++) {
        VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &frames_[i].commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext = nullptr;
        cmdAllocInfo.commandPool = frames_[i].commandPool;
        cmdAllocInfo.commandBufferCount = 1;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &frames_[i].mainCommandBuffer));
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

    for (int i = 0; i < kFrameOverlap; i++) {
        VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &frames_[i].renderFence));

        VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frames_[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frames_[i].renderSemaphore));
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
    //create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };

    globalDescriptorAllocator_.InitPool(device_, 10, sizes);

    //make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawImageDescriptorLayout_ = builder.Build(device_, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    drawImageDescriptors_ = globalDescriptorAllocator_.allocate(device_, drawImageDescriptorLayout_);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = drawImage_.imageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;

    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = drawImageDescriptors_;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device_, 1, &drawImageWrite, 0, nullptr);

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    mainDeletionQueue_.PushFunction([&]() {
        globalDescriptorAllocator_.DestroyPool(device_);

        vkDestroyDescriptorSetLayout(device_, drawImageDescriptorLayout_, nullptr);
    });
}

void SrsVkRenderer::InitPipelines() {
    InitBackgroundPipelines();
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
    if (!load_shader_module("../../src/sirius/shaders/gradient_color.comp.spv", device_, &gradientShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    VkShaderModule skyShader;
    if (!load_shader_module("../../src/sirius/shaders/sky.comp.spv", device_, &skyShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = gradientShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = gradientPipelineLayout_;
    computePipelineCreateInfo.stage = stageinfo;

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

void SrsVkRenderer::InitTrianglePipeline() {
    VkShaderModule vertShader;
    if (!load_shader_module("../../src/sirius/shaders/triangle.vert.spv", device_, &vertShader)) {
        fmt::print("Error when loading the triangle fragment shader \n");
    } else {
        fmt::print("Triangle fragment shader loaded successfully");
    }

    VkShaderModule fragShader;
    if (!load_shader_module("../../src/sirius/shaders/triangle.frag.spv", device_, &fragShader)) {
        fmt::print("Error when loading the fragment shader \n");
    } else {
        fmt::print("Triangle fragment shader loaded successfully");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = init::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &gradientPipelineLayout_));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.pipelineLayout_ = trianglePipelineLayout_;
    

}

void SrsVkRenderer::InitImgui() {
    IMGUI_CHECKVERSION();

    VkDescriptorPoolSize pool_sizes[] = {
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

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device_, &pool_info, nullptr, &imguiPool));

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

    info.PipelineInfoMain.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
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

void SrsVkRenderer::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(device_, 1, &immFence_));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer_, 0));

    VkCommandBuffer cmd = immCommandBuffer_;

    VkCommandBufferBeginInfo cmdBeginInfo = init::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = init::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = init::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, immFence_));

    VK_CHECK(vkWaitForFences(device_, 1, &immFence_, true, 9999999999));
}
}
