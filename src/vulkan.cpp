#include "pch.h"
#include "app.h"
#include "mesh.h"
#include "util.h"
#include "timer.h"
#include "entry.h"
#include "camera.h"
#include "buffer.h"
#include "PlatformContext.h"

namespace Raekor {

void Application::vulkan_main() {
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    int sdl_err = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdl_err == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto window = SDL_CreateWindow(name.c_str(),
        displays[index].x,
        displays[index].y,
        displays[index].w,
        displays[index].h,
        wflags);

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    Camera camera = Camera({ 0, 0, 4.0f }, 45.0f);

    uint32_t vk_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, nullptr);
    std::cout << vk_extension_count << '\n';

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &appInfo;

    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr)) {
        std::cout << "failed to get instance extensions" << std::endl;
    }

    std::vector<const char*> extensions = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME // Sample additional extension
    };
    size_t additional_extension_count = extensions.size();
    extensions.resize(additional_extension_count + count);

    auto sdl_bool = SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data() + additional_extension_count);
    m_assert(sdl_bool, "failed to get instance extensions");

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
    };

    if (enableValidationLayers) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool found = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            m_assert(found, "requested a vulkan validation layer that is not supported");
        }
    }

    // Now we can make the Vulkan instance
    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        instance_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instance_info.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        instance_info.enabledLayerCount = 0;
    }

    VkInstance instance;
    VkResult err = vkCreateInstance(&instance_info, nullptr, &instance);
    m_assert(err == VK_SUCCESS, "failed to create vulkan instance");

    // query SDL's machine info for the window's HWND
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(window, &wminfo);
    auto vk_hwnd = wminfo.info.win.window;

    VkSurfaceKHR vk_surface;
    bool res = SDL_Vulkan_CreateSurface(window, instance, &vk_surface);
    m_assert(res, "failed to create vulkan surface");

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    m_assert(deviceCount != 0, "failed to find GPUs with vulkan support");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // prefer dedicated GPU
    VkPhysicalDevice vk_gpu = VK_NULL_HANDLE;
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            vk_gpu = device;
            break;
        }
    }
    // else we just get the first adapter found
    // TODO: implement actual device picking and scoring
    if (!vk_gpu) vk_gpu = *devices.begin();

    const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    struct queue_indices {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool isComplete() {
            return graphics.has_value() && present.has_value();
        }
    };

    queue_indices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &queueFamilyCount, queueFamilies.data());

    int queue_index = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = queue_index;
        } 
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk_gpu, queue_index, vk_surface, &presentSupport);
        if (queueFamily.queueCount > 0 && presentSupport) {
            indices.present = queue_index;
        }
        queue_index++;
    }

    m_assert(indices.isComplete(), "device does not support graphics queue family");
    m_assert(requiredExtensions.empty(), "vk extensions not supported");

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphics.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};

    //
    // VULKAN LOGICAL DEVICE AND PRESENTATION QUEUE STAGE
    //

    VkDevice vk_device;
    VkQueue present_queue;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphics.value(), indices.present.value() };

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    device_info.pQueueCreateInfos = queueCreateInfos.data();
    device_info.pEnabledFeatures = &deviceFeatures;
    device_info.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    device_info.ppEnabledExtensionNames = deviceExtensions.data();


    if (enableValidationLayers) {
        device_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        device_info.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        device_info.enabledLayerCount = 0;
    }

    err = vkCreateDevice(vk_gpu, &device_info, nullptr, &vk_device);
    m_assert(err == VK_SUCCESS, "failed to create logical device");

    VkQueue graphics_queue;
    vkGetDeviceQueue(vk_device, indices.graphics.value(), 0, &graphics_queue);
    vkGetDeviceQueue(vk_device, indices.present.value(), 0, &present_queue);

    //
    // VULKAN SWAP CHAIN STAGE
    //
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_gpu, vk_surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_gpu, vk_surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk_gpu, vk_surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk_gpu, vk_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk_gpu, vk_surface, &presentModeCount, details.presentModes.data());
    }

    bool swapChainAdequate = false;
    if (extensions.empty()) {
        swapChainAdequate = !details.formats.empty() && !details.presentModes.empty();
    }

    const std::vector<VkSurfaceFormatKHR> availableFormats = details.formats;
    const std::vector<VkPresentModeKHR> availablePresentModes = details.presentModes;
    const VkSurfaceCapabilitiesKHR capabilities = details.capabilities;
    //TODO: fill the vector
    VkSurfaceFormatKHR surface_format = availableFormats[0];
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D swap_extent = {};

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = availableFormat;
        }
    }

    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = availablePresentMode;
        }
    }

    if (capabilities.currentExtent.width != UINT32_MAX) {
        swap_extent = capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { (uint32_t)displays[index].w, (uint32_t)displays[index].h };
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        swap_extent = actualExtent;
    }
    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }


    VkSwapchainCreateInfoKHR sc_info = {};
    sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sc_info.surface = vk_surface;

    sc_info.minImageCount = imageCount;
    sc_info.imageFormat = surface_format.format;
    sc_info.imageColorSpace = surface_format.colorSpace;
    sc_info.imageExtent = swap_extent;
    sc_info.imageArrayLayers = 1;
    sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { indices.graphics.value(), indices.present.value() };

    if (indices.graphics != indices.present) {
        sc_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sc_info.queueFamilyIndexCount = 2;
        sc_info.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sc_info.queueFamilyIndexCount = 0; // Optional
        sc_info.pQueueFamilyIndices = nullptr; // Optional
    }

    sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sc_info.preTransform = details.capabilities.currentTransform;
    sc_info.presentMode = present_mode;
    sc_info.clipped = VK_TRUE;
    sc_info.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR vk_swapchain;
    err = vkCreateSwapchainKHR(vk_device, &sc_info, nullptr, &vk_swapchain);
    m_assert(err == VK_SUCCESS, "failed to create vulkan swap chain");

    // create a set of swap chain images
    std::vector<VkImage> swapChainImages;
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &imageCount, swapChainImages.data());
    VkFormat swapChainImageFormat = surface_format.format;

    std::vector<VkImageView> swapChainImageViews;
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo iv_info = {};
        iv_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv_info.image = swapChainImages[i];
        iv_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv_info.format = swapChainImageFormat;
        iv_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv_info.subresourceRange.baseMipLevel = 0; 
        iv_info.subresourceRange.levelCount = 1;
        iv_info.subresourceRange.baseArrayLayer = 0;
        iv_info.subresourceRange.layerCount = 1;

        auto result = vkCreateImageView(vk_device, &iv_info, nullptr, &swapChainImageViews[i]);
        m_assert(result == VK_SUCCESS, "failed to create image view");
    }

    //
    // VULKAN SHADER STAGE
    //
    system("shaders\\compile.bat");

    auto read_shader = [&](const std::string& fp) {
        std::ifstream file(fp, std::ios::ate | std::ios::binary);
        m_assert(file.is_open(), "failed to open " + fp);
        size_t filesize = (size_t)file.tellg();
        std::vector<char> buffer(filesize);
        file.seekg(0);
        file.read(buffer.data(), filesize);
        file.close();
        return buffer;
    };

    auto create_shader_module = [&](const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule module;
        auto hr = vkCreateShaderModule(vk_device, &createInfo, nullptr, &module);
        m_assert(hr == VK_SUCCESS, "failed to create vk shader module");
        return module;
    };

    auto vertShaderCode = read_shader("shaders/vert.spv");
    auto fragShaderCode = read_shader("shaders/frag.spv");
    auto vertShaderModule = create_shader_module(vertShaderCode);
    auto fragShaderModule = create_shader_module(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    //
    // VULKAN VERTEX INPUT, TOPOLOGY, VIEWPORT, RASTERIZER
    //

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr_pos = {};
    attr_pos.binding = 0;
    attr_pos.location = 0;
    attr_pos.format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_pos.offset = offsetof(Vertex, pos);

    VkVertexInputAttributeDescription attr_uv = {};
    attr_uv.binding = 0;
    attr_uv.location = 1;
    attr_uv.format = VK_FORMAT_R32G32_SFLOAT;
    attr_uv.offset = offsetof(Vertex, uv);

    VkVertexInputAttributeDescription attr_normal = {};
    attr_normal.binding = 0;
    attr_normal.location = 2;
    attr_normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_normal.offset = offsetof(Vertex, normal);

    std::array<VkVertexInputAttributeDescription, Vertex::attribute_count> attribute_descriptions = {
        attr_pos, attr_uv, attr_normal
    };

    VkCommandPool commandPool;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices.graphics.value();
    err = vkCreateCommandPool(vk_device, &poolInfo, nullptr, &commandPool);
    m_assert(err == VK_SUCCESS, "failed to create vk command pool");

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vk_gpu, &memProperties);

    auto findMemoryType = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
    };

    auto createBuffer = [&](VkDeviceSize size, VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(vk_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vk_device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(vk_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(vk_device, buffer, bufferMemory, 0);
    };

    auto copyBuffer = [&](VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(vk_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue);

        vkFreeCommandBuffers(vk_device, commandPool, 1, &commandBuffer);
    };

    // describe the vertex buffer, for now nothing special
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkDeviceSize buffer_size = sizeof(Vertex) * v_cube.size();

    VkBuffer staging_buffer;
    VkDeviceMemory stage_mem;
    createBuffer(
        buffer_size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        staging_buffer, 
        stage_mem
    );

    void* data;
    vkMapMemory(vk_device, stage_mem, 0, buffer_size, 0, &data);
    memcpy(data, v_cube.data(), (size_t)buffer_size);
    vkUnmapMemory(vk_device, stage_mem);

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_mem;
    createBuffer(
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertex_buffer,
        vertex_mem
    );

    copyBuffer(staging_buffer, vertex_buffer, buffer_size);

    vkDestroyBuffer(vk_device, staging_buffer, nullptr);
    vkFreeMemory(vk_device, stage_mem, nullptr);

    VkDeviceSize indices_size = sizeof(uint32_t) * i_cube.size();

    VkBuffer stage_indices_buffer;
    VkDeviceMemory indices_stage_mem;

    createBuffer(
        indices_size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        stage_indices_buffer, 
        indices_stage_mem
    );

    void* data1;
    vkMapMemory(vk_device, indices_stage_mem, 0, indices_size, 0, &data1);
    memcpy(data1, i_cube.data(), (size_t)indices_size);
    vkUnmapMemory(vk_device, indices_stage_mem);

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    createBuffer(indices_size, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        indexBuffer, 
        indexBufferMemory);

    copyBuffer(stage_indices_buffer, indexBuffer, indices_size);

    vkDestroyBuffer(vk_device, stage_indices_buffer, nullptr);
    vkFreeMemory(vk_device, indices_stage_mem, nullptr);

    //
    //  UNIFORM BUFFERS
    //
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayout descriptorSetLayout;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    err = vkCreateDescriptorSetLayout(vk_device, &layoutInfo, nullptr, &descriptorSetLayout);
    m_assert(err == VK_SUCCESS, "failed to create descriptor layout");

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    uniformBuffers.resize(swapChainImages.size());
    uniformBuffersMemory.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(
            sizeof(cb_vs), 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            uniformBuffers[i], 
            uniformBuffersMemory[i]
        );
    }

    VkDescriptorPoolSize ubo_pool_size = {};
    ubo_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_pool_size.descriptorCount = static_cast<uint32_t>(swapChainImages.size());

    VkDescriptorPoolCreateInfo ubo_pool_info = {};
    ubo_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ubo_pool_info.poolSizeCount = 1;
    ubo_pool_info.pPoolSizes = &ubo_pool_size;
    ubo_pool_info.maxSets = static_cast<uint32_t>(swapChainImages.size());

    VkDescriptorPool descriptorPool;
    err = vkCreateDescriptorPool(vk_device, &ubo_pool_info, nullptr, &descriptorPool);
    m_assert(err == VK_SUCCESS, "failed to create descriptor pool for UBO");

    std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
    VkDescriptorSetAllocateInfo desc_info = {};
    desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    desc_info.descriptorPool = descriptorPool;
    desc_info.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    desc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.resize(swapChainImages.size());
    err = vkAllocateDescriptorSets(vk_device, &desc_info, descriptorSets.data());
    m_assert(err == VK_SUCCESS, "failed to allocate descriptor sets");

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(cb_vs);

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;

        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;
        vkUpdateDescriptorSets(vk_device, 1, &descriptorWrite, 0, nullptr);
    }

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // describe the topology used, like in directx
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // viewport and scissoring, fairly straight forward
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swap_extent.width;
    viewport.height = (float)swap_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = swap_extent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // setup the vk rasterizer, also straight forward
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    err = vkCreatePipelineLayout(vk_device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    m_assert(err == VK_SUCCESS, "failed to create pipeline layout");

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    err = vkCreateRenderPass(vk_device, &renderPassInfo, nullptr, &renderPass);
    m_assert(err == VK_SUCCESS, "failed to create vk render pass");

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline graphicsPipeline;
    err = vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
    m_assert(err == VK_SUCCESS, "failed to create vk final graphics pipeline");

    std::vector<VkFramebuffer> swapChainFramebuffers;
    swapChainFramebuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swap_extent.width;
        framebufferInfo.height = swap_extent.height;
        framebufferInfo.layers = 1;
    
        auto r = vkCreateFramebuffer(vk_device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
        m_assert(r == VK_SUCCESS, "failed to create vk framebuffer");
    }

    std::vector<VkCommandBuffer> commandBuffers;
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo cmd_allocInfo = {};
    cmd_allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_allocInfo.commandPool = commandPool;
    cmd_allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    err = vkAllocateCommandBuffers(vk_device, &cmd_allocInfo, commandBuffers.data());
    m_assert(err == VK_SUCCESS, "failed to allocate vk command buffers");

    for (size_t i = 0; i < commandBuffers.size(); i++) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        auto hr = vkBeginCommandBuffer(commandBuffers[i], &beginInfo);
        m_assert(hr == VK_SUCCESS, "failed to record command buffer");

        VkRenderPassBeginInfo render_info = {};
        render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_info.renderPass = renderPass;
        render_info.framebuffer = swapChainFramebuffers[i];
        render_info.renderArea.offset = { 0, 0 };
        render_info.renderArea.extent = swap_extent;

        VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        render_info.clearValueCount = 1;
        render_info.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers[i], &render_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkBuffer vertexBuffers[] = { vertex_buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

        vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(i_cube.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffers[i]);

        err = vkEndCommandBuffer(commandBuffers[i]);
        m_assert(err == VK_SUCCESS, "failed to end command buffer");

    }

    constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    size_t current_frame = 0;
    std::vector<VkFence> inFlightFences;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto hr = vkCreateSemaphore(vk_device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        m_assert(hr == VK_SUCCESS, "failed to create vk semaphore");
        hr = vkCreateSemaphore(vk_device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        m_assert(hr == VK_SUCCESS, "failed to create vk semaphore");
        hr = vkCreateFence(vk_device, &fenceInfo, nullptr, &inFlightFences[i]);
        m_assert(hr == VK_SUCCESS, "failed to create vk fence");
    }

    std::puts("job well done.");

    SDL_SetWindowInputFocus(window);

    bool running = true;

    // MVP uniform buffer object
    cb_vs ubo;
    
    Timer dt_timer;
    double dt = 0;
    //main application loop
    while (running) {
        dt_timer.start();
        //handle sdl and imgui events
        handle_sdl_gui_events({ window }, camera, dt);

        vkWaitForFences(vk_device, 1, &inFlightFences[current_frame], VK_TRUE, UINT64_MAX);
        vkResetFences(vk_device, 1, &inFlightFences[current_frame]);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(vk_device, vk_swapchain, UINT64_MAX, imageAvailableSemaphores[current_frame], VK_NULL_HANDLE, &imageIndex);

        // update uniform buffer data
        camera.update(glm::mat4(1.0f));
        ubo.MVP = camera.get_mvp(false);
        void* data;
        vkMapMemory(vk_device, uniformBuffersMemory[current_frame], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(vk_device, uniformBuffersMemory[current_frame]);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[current_frame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[current_frame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        err = vkQueueSubmit(graphics_queue, 1, &submitInfo, inFlightFences[current_frame]);
        m_assert(err == VK_SUCCESS, "failed to submit queue");

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { vk_swapchain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional
        vkQueuePresentKHR(present_queue, &presentInfo);

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        dt_timer.stop();
        dt = dt_timer.elapsed_ms();
    }
    vkDeviceWaitIdle(vk_device);

    vkDestroyInstance(instance, nullptr);
    vkDestroyDevice(vk_device, nullptr);
    vkDestroySurfaceKHR(instance, vk_surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
    for (auto& imageview : swapChainImageViews) 
        vkDestroyImageView(vk_device, imageview, nullptr);
    for (auto& fb : swapChainFramebuffers) 
        vkDestroyFramebuffer(vk_device, fb, nullptr);
    vkDestroyShaderModule(vk_device, vertShaderModule, nullptr);
    vkDestroyShaderModule(vk_device, fragShaderModule, nullptr);
    vkDestroyPipelineLayout(vk_device, pipelineLayout, nullptr);
    vkDestroyPipeline(vk_device, graphicsPipeline, nullptr);
    vkDestroyRenderPass(vk_device, renderPass, nullptr);
    vkDestroyCommandPool(vk_device, commandPool, nullptr);
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(vk_device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(vk_device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(vk_device, inFlightFences[i], nullptr);
    }
    vkDestroyBuffer(vk_device, vertex_buffer, nullptr);
    vkFreeMemory(vk_device, vertex_mem, nullptr);
    vkDestroyBuffer(vk_device, indexBuffer, nullptr);
    vkFreeMemory(vk_device, indexBufferMemory, nullptr);
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vkDestroyBuffer(vk_device, uniformBuffers[i], nullptr);
        vkFreeMemory(vk_device, uniformBuffersMemory[i], nullptr);
    }
    vkDestroyDescriptorPool(vk_device, descriptorPool, nullptr);
}

} // namespace Raekor