//
// Created by Leon on 23/09/2025.
//

#pragma once
#include <vulkan/vulkan.h>

namespace sirius {
class utils {
public:
    struct transitionFlags {
        VkPipelineStageFlags srcStageMask;
        VkAccessFlags2 srcAccessMask;
        VkPipelineStageFlags dstStageMask;
        VkAccessFlags2 dstAccessMask;
    };
    static void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, transitionFlags flags);

    static void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcExtend, VkExtent2D dstExtend);
};
}
