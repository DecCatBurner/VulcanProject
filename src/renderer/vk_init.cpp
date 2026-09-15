#pragma once
#include <vulkan/vulkan.h>

#define VK_CHECK(result) do {                              \
if (result != VK_SUCCESS)                                  \
{                                                          \
    std::cout << "VULKAN ERROR: " << result << std::endl;  \
    __debugbreak();                                        \
    return false;                                          \
}                                                          \
} while (0)

VkCommandBufferAllocateInfo cmd_allocate_buffer_info(VkCommandPool pool) {
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandBufferCount = 1;
    info.commandPool = pool;

    return info;
}

VkFenceCreateInfo fence_info(VkFenceCreateFlags flags = 0) {
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = flags;

    return info;
}

VkSubmitInfo submit_info(VkCommandBuffer* commandBuffer, uint32_t count = 1) {
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = count;
    info.pCommandBuffers = commandBuffer;

    return info;
}