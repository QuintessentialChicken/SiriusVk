//
// Created by Lms on 29/09/2025.
//

#pragma once

#include <vulkan/vulkan_core.h>


namespace init {
VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);

VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);

VkRenderingAttachmentInfo attachment_info(VkImageView view, VkClearValue* clear ,VkImageLayout layout);

VkRenderingInfo rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment);
} // init
