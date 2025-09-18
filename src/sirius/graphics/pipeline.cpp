// //
// // Created by Leon on 17/09/2025.
// //
//
// #include "pipeline.h"
//
// #include <cassert>
// #include <fstream>
// #include <stdexcept>
//
// namespace sirius {
// srsPipeline::srsPipeline(srsDevice device, const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo) : device{device} {
//     createGraphicsPipeline(vertFilepath, fragFilepath, configInfo);
// }
//
// srsPipeline::~srsPipeline() {
//     vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
//     vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
//     vkDestroyPipeline(device.device(), graphicsPipeline, nullptr);
// }
//
// PipelineConfigInfo srsPipeline::getDefaultPipelineConfigInfo(uint32_t width, uint32_t height) {
//     PipelineConfigInfo pipelineConfigInfo;
//
//     return pipelineConfigInfo;
// }
//
// std::vector<char> srsPipeline::readFile(const std::string& filepath) {
//     std::ifstream file{filepath, std::ios::ate | std::ios::binary};
//
//     if (!file.is_open()) {
//         throw std::runtime_error("failed to open file: " + filepath);
//     }
//
//     size_t fileSize = file.tellg();
//     std::vector<char> buffer(fileSize);
//
//     file.seekg(0);
//     file.read(buffer.data(), fileSize);
//
//     file.close();
//     return buffer;
// }
//
// void srsPipeline::createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo) {
//     assert(
//         configInfo.pipelineLayout != nullptr &&
//         "Cannot create graphics pipeline: no pipelineLayout provided in config info");
//     assert(
//         configInfo.renderPass != nullptr &&
//         "Cannot create graphics pipeline: no renderPass provided in config info");
//
//     auto vertCode = readFile(vertFilepath);
//     auto fragCode = readFile(fragFilepath);
//
//     createShaderModule(vertCode, &vertShaderModule);
//     createShaderModule(fragCode, &fragShaderModule);
//
//     VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
//     vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//     vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
//     vertShaderStageInfo.module = vertShaderModule;
//     vertShaderStageInfo.pName = "main";
//
//     VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
//     fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//     fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
//     fragShaderStageInfo.module = fragShaderModule;
//     fragShaderStageInfo.pName = "main";
//
//     VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
//
//     VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
//     vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//     vertexInputInfo.vertexBindingDescriptionCount = 0;
//     vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
//     vertexInputInfo.vertexAttributeDescriptionCount = 0;
//     vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional
//
//     VkGraphicsPipelineCreateInfo pipelineInfo = {};
//     pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
//     pipelineInfo.stageCount = 2;
//     pipelineInfo.pStages = shaderStages;
//
//     VkPipelineViewportStateCreateInfo viewportState = {};
//     viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
//     viewportState.viewportCount = 1;
//     viewportState.pViewports = &configInfo.viewport;
//     viewportState.scissorCount = 1;
//     viewportState.pScissors = &configInfo.scissor;
//
//     pipelineInfo.pVertexInputState = &vertexInputInfo;
//     pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
//     pipelineInfo.pViewportState = &viewportState;
//     pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
//     pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
//     pipelineInfo.pDepthStencilState = nullptr; // Optional
//     pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
//     pipelineInfo.pDynamicState = nullptr; // Optional
//     pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
//
//     pipelineInfo.layout = configInfo.pipelineLayout;
//     pipelineInfo.renderPass = configInfo.renderPass;
//     pipelineInfo.subpass = configInfo.subpass;
//
//     pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
//     pipelineInfo.basePipelineIndex = -1; // Optional
//
//     pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
//     pipelineInfo.basePipelineIndex = -1; // Optional
//
//     if (vkCreateGraphicsPipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo,  nullptr, &graphicsPipeline) != VK_SUCCESS) {
//         throw std::runtime_error("failed to create graphics pipeline!");
//     }
//
//     vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
//     vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
//     fragShaderModule = VK_NULL_HANDLE;
//     vertShaderModule = VK_NULL_HANDLE;
// }
//
// void srsPipeline::createShaderModule(const std::vector<char>& shaderCode, VkShaderModule* shaderModule) {
//     VkShaderModuleCreateInfo createInfo{};
//     createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//     createInfo.codeSize = shaderCode.size();
//     createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
//
//     if (vkCreateShaderModule(device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
//         throw std::runtime_error("failed to create shader module");
//     }
// }
// }
