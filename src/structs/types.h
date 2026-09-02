#pragma once

#include <vulkan/vulkan.h>

struct LoadedImage {
    VkImage image;
    VkDeviceMemory memory;
};