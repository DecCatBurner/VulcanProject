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
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkDebugUtilsMessengerEXT debugMessenger;

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

    /*
     * Create the VK Debug Extension 
     */

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

    /* 
     * Create the VK Surface 
     */
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

    /* 
     * Choose the Physical Device | GPU 
     */
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

    /* 
     * Create the Logical Device 
     */
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

    /* 
     * Create the Swap Chain 
     */
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

    std::cout << "Initialized Vulkan" << std::endl;

    return true;
}