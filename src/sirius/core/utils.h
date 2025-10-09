//
// Created by Leon on 23/09/2025.
//

#pragma once
#include <vulkan/vulkan.h>

namespace sirius {
class Utils {
public:
    struct TransitionFlags {
        VkPipelineStageFlags srcStageMask;
        VkAccessFlags2 srcAccessMask;
        VkPipelineStageFlags dstStageMask;
        VkAccessFlags2 dstAccessMask;
    };
    static void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, TransitionFlags flags);

    static void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcExtend, VkExtent2D dstExtend);
};
}
