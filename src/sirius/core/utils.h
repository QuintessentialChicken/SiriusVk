//
// Created by Leon on 23/09/2025.
//

#pragma once
#include <vulkan/vulkan.h>

namespace sirius {
class utils {
public:
    static void transitionImage(VkDevice device, VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

    static void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcExtend, VkExtent2D dstExtend);
};
}
