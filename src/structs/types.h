#pragma once

#include <vulkan/vulkan.h>

struct LoadedImage {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* data;
};