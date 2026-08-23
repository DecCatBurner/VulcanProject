#include <iostream>
#include <vulkan/vulkan.h>
#include <windows.h>
#ifdef WIN32_BUILD
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#else                                                  \
std::cout << "ERROR: No defined system." << std::endl;
#endif

#define ARRAY_SIZE(arr) (sizeof((arr))/sizeof((arr)[0]))

#define VK_CHECK(result) do {                              \
if (result != VK_SUCCESS)                                  \
{                                                          \
    std::cout << "VULKAN ERROR: " << result << std::endl;  \
    __debugbreak();                                        \
    return false;                                          \
} } while (0)

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    std::cout << "ERROR: " << pCallbackData->pMessage << std::endl;
    return false;
}

struct VkContext {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapchain;
    VkImage* swapchainImages = new VkImage[0];
    VkCommandPool commandPool;

    VkSemaphore aquireSemaphore;
    VkSemaphore submitSemaphore;

    uint32_t swapchainImageCount;
    int graphicsIndex;
};

bool Vk_Init(VkContext* vkcontext, void* window) {
    std::cout << "Initializing Vulkan..." << std::endl;

    /* 
     * Print Info 
     */
    std::cout << VK_HEADER_VERSION_COMPLETE << std::endl;
    std::cout << "API: " << VK_API_VERSION_1_0 << std::endl;

    /* 
     * Create the VK Application 
     */
    {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Some Game, IDK";
    appInfo.pEngineName = "Vulkan Engine";

    /*
     * Define the Extensions for VK Instance 
     */
    const char* extensions[] = {
    #ifdef WIN32_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #elif LINUX_BUILD
    #endif
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    const char* layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    /* 
     * Create the VK Instance 
     */
    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions;
    instanceInfo.enabledExtensionCount = ARRAY_SIZE(extensions);
    instanceInfo.ppEnabledLayerNames = layers;
    instanceInfo.enabledLayerCount = ARRAY_SIZE(layers);
    VK_CHECK(vkCreateInstance(&instanceInfo, 0, &vkcontext->instance));
    }

    /*
     * Create the VK Debug Extension 
     */
    {
    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkcontext->instance, "vkCreateDebugUtilsMessengerEXT");

    if(vkCreateDebugUtilsMessengerEXT) {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugInfo.pfnUserCallback = vk_debug_callback;

        vkCreateDebugUtilsMessengerEXT(vkcontext->instance, &debugInfo, 0, &vkcontext->debugMessenger);
    } else {
        return false;
    }
    }

    /* 
     * Create the VK Surface 
     */
    {
#ifdef WIN32_BUILD
    std::cout << "WIN32 Build" << std::endl;
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hwnd = (HWND)window;
    surfaceInfo.hinstance = GetModuleHandleA(0);
    VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, 0, &vkcontext->surface));
    std::cout << "Created HWND = " << window << std::endl;
    std::cout << "IsWindow = " << IsWindow((HWND)window) << std::endl;
#elif LINUX_BUILD
#endif
    std::cout << "VK Surface: " << vkcontext->surface << std::endl;
    }

    /* 
     * Choose the Physical Device | GPU 
     */
    {
    vkcontext->graphicsIndex = -1;
    uint32_t gpuCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, 0));
    //TODO: Suballoation from Main Allocation
    VkPhysicalDevice* gpus = new VkPhysicalDevice[gpuCount];
    VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

    for (uint32_t i = 0; i < gpuCount; i++) {
        VkPhysicalDevice gpu = gpus[i];

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);
        //TODO: Suballoation from Main Allocation
        VkQueueFamilyProperties* queueProps = new VkQueueFamilyProperties[queueFamilyCount];
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProps);

        for (uint32_t j = 0; j < queueFamilyCount; j++) {
            if(queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 surfaceSupported = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkcontext->surface, &surfaceSupported));

                if (surfaceSupported) { 
                    vkcontext->graphicsIndex = j; 
                    vkcontext->gpu = gpu;
                    break;
                }
            }
        }

        delete[] queueProps;
    }

    delete[] gpus;

    if (vkcontext->graphicsIndex < 0) {
        return false;
    }

    std::cout << "VK GPU: " << vkcontext->gpu << std::endl;
    }

    /* 
     * Create the Logical Device 
     */
    {
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = vkcontext->graphicsIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    const char* extensionsDevice[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.ppEnabledExtensionNames = extensionsDevice;
    deviceInfo.enabledExtensionCount = ARRAY_SIZE(extensionsDevice);
    VK_CHECK(vkCreateDevice(vkcontext->gpu, &deviceInfo, 0, &vkcontext->device));

    vkGetDeviceQueue(vkcontext->device, vkcontext->graphicsIndex, 0, &vkcontext->graphicsQueue);
    }

    /* 
     * Create the Swap Chain 
     */
    {
    uint32_t surfaceFormatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &surfaceFormatCount, 0));
    // Suballocate from main memory
    VkSurfaceFormatKHR* surfaceFormats = new VkSurfaceFormatKHR[surfaceFormatCount];
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &surfaceFormatCount, surfaceFormats));

    for (uint32_t i = 0; i < surfaceFormatCount; i++) {
        VkSurfaceFormatKHR surfaceFormat = surfaceFormats[i];
        
        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB) {
            vkcontext->surfaceFormat = surfaceFormat;
        }
    }

    delete[] surfaceFormats;

    VkSurfaceCapabilitiesKHR surfaceCapabilitiesInfo = {};

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCapabilitiesInfo));
    uint32_t imageCount = surfaceCapabilitiesInfo.minImageCount + 1;
    if (imageCount > surfaceCapabilitiesInfo.maxImageCount) imageCount--;
    
    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.surface = vkcontext->surface;
    swapchainInfo.imageFormat = vkcontext->surfaceFormat.format;
    swapchainInfo.preTransform = surfaceCapabilitiesInfo.currentTransform;
    swapchainInfo.imageExtent = surfaceCapabilitiesInfo.currentExtent;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageArrayLayers = 1;

    VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &swapchainInfo, 0, &vkcontext->swapchain));

    /* Establish Swapchain Images */

    VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->swapchainImageCount, 0));
    VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->swapchainImageCount, vkcontext->swapchainImages));
    }

    /*
     * Create the Command Pool
     */
    {
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    VK_CHECK(vkCreateCommandPool(vkcontext->device, &commandPoolInfo, 0, &vkcontext->commandPool));
    }

    /*
     * Create the VK Semaphores
     */
    {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaphoreInfo, 0, &vkcontext->aquireSemaphore));
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaphoreInfo, 0, &vkcontext->submitSemaphore));
    }

    std::cout << "Initialized Vulkan" << std::endl;

    return true;
}

bool vk_render(VkContext* vkcontext) {
    uint32_t imageIndex;
    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, 0, vkcontext->aquireSemaphore, 0, &imageIndex));

    /* 
     * Create Command Buffer
     */
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool = vkcontext->commandPool;
    VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocateInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    /*
     * Rendering Pass
     */
    {
        VkClearColorValue backgroundColor = {0.66,0.33,0,1};
        VkImageSubresourceRange range = {};
        range.layerCount = 1;
        range.levelCount = 1;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdClearColorImage(commandBuffer, vkcontext->swapchainImages[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &backgroundColor, 1, &range);
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vkcontext->submitSemaphore;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkcontext->aquireSemaphore;
    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, 0));
    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &vkcontext->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vkcontext->submitSemaphore;
    VK_CHECK(vkQueuePresentKHR(vkcontext->graphicsQueue, &presentInfo));
    
    VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

    vkFreeCommandBuffers(vkcontext->device, vkcontext->commandPool, 1, &commandBuffer);

    return true;
}