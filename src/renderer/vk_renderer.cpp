#include <iostream>
#include <vulkan/vulkan.h>
#include <windows.h>
#ifdef WIN32_BUILD
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#else                                                  \
std::cout << "ERROR: No defined system." << std::endl;
#endif
#include "structs/dds.h"
#include "structs/types.h"
#include "platform.h"
#include "vk_init.cpp"

#define ARRAY_SIZE(arr) (sizeof((arr))/sizeof((arr)[0]))

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    std::cout << "ERROR: " << pCallbackData->pMessage << std::endl;
    return false;
}

struct VkContext {
    VkImage* swapchainImages = new VkImage[0];
    VkImageView* swapchainImageViews = new VkImageView[0];
    VkFramebuffer* frameBuffer = new VkFramebuffer[0];

    VkExtent2D screenSize;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;

    VkCommandPool commandPool;
    VkDescriptorPool descriptorPool;

    VkSampler sampler;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet descriptorSet;

    VkSemaphore aquireSemaphore;
    VkSemaphore submitSemaphore;
    VkFence imageAvaliableFence;

    Buffer stagingBuffer;
    LoadedImage image;

    uint32_t swapchainImageCount;
    int graphicsIndex;
};

bool Vk_Init(VkContext* vkcontext, void* window) {
    std::cout << "Initializing Vulkan..." << std::endl;

    platform_get_window_size(&vkcontext->screenSize.width, &vkcontext->screenSize.height);

    /* 
     * Print Info 
     */
    std::cout << VK_HEADER_VERSION_COMPLETE << std::endl;
    std::cout << "API: " << VK_API_VERSION_1_0 << std::endl;

    /* 
     * Create the VK Application 
     */
    {
    std::cout << "Creating the Application..." << '\n';

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
    std::cout << "Creating the Debugger..." << '\n';
    
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
    std::cout << "Creating the Surface..." << '\n';
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
    std::cout << "Initializing the GPU..." << '\n';

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
    std::cout << "Initializing the Logical Device..." << '\n';
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
     * Create the Swapchain 
     */
    {
    std::cout << "Creating the Swapchain..." << '\n';

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
    vkcontext->swapchainImages = new VkImage[vkcontext->swapchainImageCount];
    VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->swapchainImageCount, vkcontext->swapchainImages));

    /* Establish Swapchain Image Views */
    VkImageViewCreateInfo imageViewInfo = {};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.format = vkcontext->surfaceFormat.format;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.layerCount = 1;
    imageViewInfo.subresourceRange.levelCount = 1;

    vkcontext->swapchainImageViews = new VkImageView[vkcontext->swapchainImageCount];
    for (int i = 0; i < vkcontext->swapchainImageCount; i++) {
        imageViewInfo.image = vkcontext->swapchainImages[i];
        VK_CHECK(vkCreateImageView(vkcontext->device, &imageViewInfo, 0, &vkcontext->swapchainImageViews[i]));
    }
    }

    /*
     * Create VK Render Pass
     */
    {
    std::cout << "Creating the Renderpass..." << '\n';

    VkAttachmentDescription attachments[] = {
        {
            NULL,                            // flags
            vkcontext->surfaceFormat.format, // format
            VK_SAMPLE_COUNT_1_BIT,           // samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,     // loadOp
            VK_ATTACHMENT_STORE_OP_STORE,    // storeOp
            (VkAttachmentLoadOp)NULL,        // stencilLoadOp
            (VkAttachmentStoreOp)NULL,       // stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,       // initialLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR  // finalLayout
        }
    };

    VkAttachmentReference colorAttachment = {};
    colorAttachment.attachment = 0;
    colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pColorAttachments = &colorAttachment;
    subpass.colorAttachmentCount = 1;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.attachmentCount = ARRAY_SIZE(attachments);
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.subpassCount = 1;

    VK_CHECK(vkCreateRenderPass(vkcontext->device, &renderPassInfo, 0, &vkcontext->renderPass));
    }

    /*
     * Create the Frame Buffer
     */
    {
    std::cout << "Creating the Frame Buffer..." << '\n';
    
    VkFramebufferCreateInfo frameBufferInfo = {};
    frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferInfo.width = vkcontext->screenSize.width;
    frameBufferInfo.height = vkcontext->screenSize.height;
    frameBufferInfo.renderPass = vkcontext->renderPass;
    frameBufferInfo.layers = 1;
    frameBufferInfo.attachmentCount = 1;

    for (int i = 0; i < vkcontext->swapchainImageCount; i++) {
        frameBufferInfo.pAttachments = &vkcontext->swapchainImageViews[i];
        VK_CHECK(vkCreateFramebuffer(vkcontext->device, &frameBufferInfo, 0, &vkcontext->frameBuffer[i]));
    }
    }

    /*
     * Create the Command Pool
     */
    {
    std::cout << "Creating the Command Pool..." << '\n';

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    VK_CHECK(vkCreateCommandPool(vkcontext->device, &commandPoolInfo, 0, &vkcontext->commandPool));
    }

    /*
     * Create the Semaphores and Fences
     */
    {
    std::cout << "Initializing Semaphores..." << '\n';

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaphoreInfo, 0, &vkcontext->aquireSemaphore));
    VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaphoreInfo, 0, &vkcontext->submitSemaphore));

    VkFenceCreateInfo fenceInfo = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, 0, &vkcontext->imageAvaliableFence));
    }

    /*
     * Create the Descriptor Set Layouts
     */
    {
        std::cout << "Creating the Descriptor Set Layout..." << '\n';

        VkDescriptorSetLayoutBinding bindingSprite = {};
        bindingSprite.binding = 0;
        bindingSprite.descriptorCount = 1; // Array size of the data
        bindingSprite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingSprite.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Stage of pipeline accessed

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1; // What's used in the shader (binding = X)
        layoutInfo.pBindings = &bindingSprite;

        VK_CHECK(vkCreateDescriptorSetLayout(vkcontext->device, &layoutInfo, 0, &vkcontext->setLayout));
    }

    /*
     * Create the Pipeline Layout
     */
    {
        std::cout << "Creating the Pipeline Layout..." << '\n';

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &vkcontext->setLayout;
        VK_CHECK(vkCreatePipelineLayout(vkcontext->device, &pipelineLayoutInfo, 0, &vkcontext->pipelineLayout));
    }

    /*
     * Create the Pipeline
     */
    {
    std::cout << "Creating the Pipeline..." << '\n';
    
    /* Vertex Input State */ std::cout << "\tVertex" << '\n';
    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    /* Color Blend State */ std::cout << "\tBlend" << '\n';
    VkPipelineColorBlendAttachmentState colorAttachment = {};
    colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.pAttachments = &colorAttachment;
    colorBlendState.attachmentCount = 1;

    /* Rasterization State */ std::cout << "\tRasterize" << '\n';
    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.lineWidth = 1.0f;

    /* Viewport State */ std::cout << "\tViewport" << '\n';
    VkRect2D scissor = {};
    VkViewport viewport = {};

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pScissors = &scissor;
    viewportState.scissorCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.viewportCount = 1;

    /* Multisample State */ std::cout << "\tMultisample" << '\n';
    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    /* Input Assembly */ std::cout << "\tAssembly" << '\n';
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    /* Shader Modules */ std::cout << "\tShader" << '\n';
    VkShaderModule vertexShader, fragmentShader;

    uint64_t sizeInBytes;
    char* code;

    VkShaderModuleCreateInfo shaderInfo = {};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    code = platform_read_file("assets/shaders/shader.vert.spv", &sizeInBytes);
    shaderInfo.pCode = (uint32_t*)code;
    shaderInfo.codeSize = sizeInBytes;
    VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &vertexShader));
    delete[] code;

    code = platform_read_file("assets/shaders/shader.frag.spv", &sizeInBytes);
    shaderInfo.pCode = (uint32_t*)code;
    shaderInfo.codeSize = sizeInBytes;
    VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &fragmentShader));
    delete[] code;

    VkPipelineShaderStageCreateInfo vertexShaderInfo = {};
    vertexShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderInfo.module = vertexShader;
    vertexShaderInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderInfo = {};
    fragmentShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderInfo.module = fragmentShader;
    fragmentShaderInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaders[] {
        vertexShaderInfo,
        fragmentShaderInfo
    };

    /* Dynamic States Declaration */ std::cout << "\tDynamic States" << '\n';
    VkDynamicState dynamicStateArray[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateArray;
    dynamicState.dynamicStateCount = ARRAY_SIZE(dynamicStateArray);

    /* Final Declaration */ std::cout << "\tFinalizing" << '\n';
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = vkcontext->pipelineLayout;
    pipelineInfo.renderPass = vkcontext->renderPass;
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pRasterizationState = &rasterizationState;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pMultisampleState = &multisampleState;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pStages = shaders;
    pipelineInfo.stageCount = ARRAY_SIZE(shaders);

    VK_CHECK(vkCreateGraphicsPipelines(vkcontext->device, 0, 1, &pipelineInfo, 0, &vkcontext->pipeline));

    vkDestroyShaderModule(vkcontext->device, vertexShader, 0);
    vkDestroyShaderModule(vkcontext->device, fragmentShader, 0);
    std::cout << "\tFinished" << std::endl;
    }

    /*
     * Create Staging Buffer
     */
    {
        std::cout << "Creating the Staging Buffer..." << '\n';

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.size = MB(1);

        VK_CHECK(vkCreateBuffer(vkcontext->device, &bufferInfo, 0, &vkcontext->stagingBuffer.buffer));

        VkMemoryRequirements memoryReqs;
        vkGetBufferMemoryRequirements(vkcontext->device, vkcontext->stagingBuffer.buffer, &memoryReqs);

        VkPhysicalDeviceMemoryProperties gpuMemoryProps;
        vkGetPhysicalDeviceMemoryProperties(vkcontext->gpu, &gpuMemoryProps);

        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = bufferInfo.size;

        for (uint32_t i = 0; i < gpuMemoryProps.memoryTypeCount; i++) {
            if (memoryReqs.memoryTypeBits & (1 << i) &&
                (gpuMemoryProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) 
                 == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                allocateInfo.memoryTypeIndex = i;
                break;
            }
        }

        VK_CHECK(vkAllocateMemory(vkcontext->device, &allocateInfo, 0, &vkcontext->stagingBuffer.memory));

        VK_CHECK(vkMapMemory(vkcontext->device, vkcontext->stagingBuffer.memory, 0, bufferInfo.size, 0, &vkcontext->stagingBuffer.data));
        VK_CHECK(vkBindBufferMemory(vkcontext->device, vkcontext->stagingBuffer.buffer, vkcontext->stagingBuffer.memory, 0));
    }

    /*
     * Create Image
     */
    {
        std::cout << "Image" << '\n';

        uint64_t filesize = 0;
        DDSFile* textureFile = (DDSFile*)platform_read_file("assets/textures/HeartBreak.DDS", &filesize);
        uint64_t textureSize = textureFile->header.width * textureFile->header.height * 4;

        memcpy(vkcontext->stagingBuffer.data, &textureFile->dataBegin, textureSize);

        //TODO: Assertions

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8_UNORM;
        imageInfo.extent = {textureFile->header.width, textureFile->header.height, 1};
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        // imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vkCreateImage(vkcontext->device, &imageInfo, 0, &vkcontext->image.image));

        VkMemoryRequirements memoryReqs;
        vkGetImageMemoryRequirements(vkcontext->device, vkcontext->image.image, &memoryReqs);

        VkPhysicalDeviceMemoryProperties gpuMemoryProps;
        vkGetPhysicalDeviceMemoryProperties(vkcontext->gpu, &gpuMemoryProps);

        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = textureSize;

        for (uint32_t i = 0; i < gpuMemoryProps.memoryTypeCount; i++) {
            if (memoryReqs.memoryTypeBits & (1 << i) &&
                (gpuMemoryProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                allocateInfo.memoryTypeIndex = i;
                break;
            }
        }

        VK_CHECK(vkAllocateMemory(vkcontext->device, &allocateInfo, 0, &vkcontext->image.memory));
        VK_CHECK(vkBindImageMemory(vkcontext->device, vkcontext->image.image, vkcontext->image.memory, 0));

        /*
         * Run Cmd Buffer
         */
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo allocateCmdInfo = cmd_allocate_buffer_info(vkcontext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocateCmdInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        /*
         * Transition Image Layout
         */
        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.layerCount = 1;
        range.levelCount = 1;

        VkImageMemoryBarrier imageMemoryBarrier= {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        imageMemoryBarrier.image = vkcontext->image.image;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.subresourceRange = range;

        vkCmdPipelineBarrier(commandBuffer, 
            VK_PIPELINE_STAGE_TRANSFER_BIT, 
            VK_PIPELINE_STAGE_TRANSFER_BIT, 
            0, 
            0, 0, 
            0, 0, 
            1, &imageMemoryBarrier);

        VkBufferImageCopy copyRegion = {};
        copyRegion.imageExtent = {textureFile->header.width, textureFile->header.height, 1};
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdCopyBufferToImage(commandBuffer, vkcontext->stagingBuffer.buffer, vkcontext->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
            0, 
            0, 0, 
            0, 0, 
            1, &imageMemoryBarrier);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkFence uploadFence;
        VkFenceCreateInfo fenceInfo = fence_info();
        VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, 0, &uploadFence));

        VkSubmitInfo submitInfo = submit_info(&commandBuffer);
        VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, uploadFence));

        VK_CHECK(vkWaitForFences(vkcontext->device, 1, &uploadFence, VK_TRUE, UINT64_MAX));
    }

    /*
     * Create Image View
     */
    {
        std::cout << "Image View..." << '\n';

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vkcontext->image.image;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        vkCreateImageView(vkcontext->device, &viewInfo, 0, &vkcontext->image.view);
    }

    /*
     * Create Sampler
     */
    {
        std::cout << "Creating the Sampler..." << '\n';

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.magFilter = VK_FILTER_NEAREST;

        vkCreateSampler(vkcontext->device, &samplerInfo, 0, &vkcontext->sampler);
    }

    /*
     * Create Descriptor Pool
     */
    {
        std::cout << "Creating the Descriptor Pool..." << '\n';

        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.maxSets = 1;
        descriptorPoolInfo.poolSizeCount = 1;
        descriptorPoolInfo.pPoolSizes = &poolSize;
        VK_CHECK(vkCreateDescriptorPool(vkcontext->device, &descriptorPoolInfo, 0, &vkcontext->descriptorPool));
    }

    /*
     * Create Descriptor Set
     */
    {
        std::cout << "Creating the Descriptor Set..." << '\n';

        VkDescriptorSetAllocateInfo setInfo = {};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setInfo.pSetLayouts = &vkcontext->setLayout;
        setInfo.descriptorSetCount = 1;
        setInfo.descriptorPool = vkcontext->descriptorPool;
        VK_CHECK(vkAllocateDescriptorSets(vkcontext->device, &setInfo, &vkcontext->descriptorSet));
    }

    /*
     * Update the Descriptor Set
     */
    {
        std::cout << "Updating the Descriptor Set..." << '\n';

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = vkcontext->image.view;
        imageInfo.sampler = vkcontext->sampler;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = vkcontext->descriptorSet;
        write.pImageInfo = &imageInfo;
        write.dstBinding = 0; // TODO: Link to descriptor set layout info
        write.descriptorCount = 1; // TODO: Link to descriprot set layout info (length of data array)
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        vkUpdateDescriptorSets(vkcontext->device, 1, &write, 0, 0);
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
    VkCommandBufferAllocateInfo allocateInfo = cmd_allocate_buffer_info(vkcontext->commandPool);
    VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocateInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    /*
     * Rendering Pass
     */
    {
        VkClearValue clearValue = {};
        clearValue.color = {0,0.89,0,1};

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = vkcontext->renderPass;
        renderPassBeginInfo.renderArea.extent = vkcontext->screenSize;
        renderPassBeginInfo.framebuffer = vkcontext->frameBuffer[imageIndex];
        renderPassBeginInfo.pClearValues = &clearValue;
        renderPassBeginInfo.clearValueCount = 1;

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkRect2D scissor = {};
        scissor.extent = vkcontext->screenSize;

        VkViewport viewport = {};
        viewport.height = vkcontext->screenSize.height;
        viewport.width = vkcontext->screenSize.width;
        viewport.maxDepth = 1.0f;

        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdSetViewport(commandBuffer, 0 ,1, &viewport);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipelineLayout,
                                0, 1, &vkcontext->descriptorSet, 0, 0);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeline);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = submit_info(&commandBuffer);
    submitInfo.pWaitDstStageMask = &waitStageMask;
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