#include "pch.h"
#include "VKBase.h"
#include "VKUtil.h"

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, 
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
) {
    std::cerr << pCallbackData->pMessage << '\n';

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        assert(false);
    }

    return VK_FALSE;
}



static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = PFN_vkDestroyDebugUtilsMessengerEXT(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}



VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = PFN_vkCreateDebugUtilsMessengerEXT(vkGetInstanceProcAddr(instance,
        "vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } 	else 	{
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}



namespace Raekor::VK {

Instance::Instance(SDL_Window* window) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName = "Raekor";
    appInfo.pApplicationName = "Raekor Editor";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 2, 0);
    appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr)) {
        throw std::runtime_error("failed to get vulkan instance extensions");
    }

    std::vector<const char*> extensions = { 
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,

    };

    const size_t additionalExtensionCount = extensions.size();
    extensions.resize(additionalExtensionCount + count);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data() + additionalExtensionCount)) {
        throw std::runtime_error("failed to get instance extensions");
    }
    
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

#if RAEKOR_DEBUG
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.pfnUserCallback = debugCallback;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (auto layerName : validationLayers) {
            bool found = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) throw std::runtime_error("requested validation layer not supported");
        }

        instanceInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugInfo;
#endif

    instanceInfo.enabledExtensionCount = uint32_t(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();

#if RAEKOR_DEBUG
        instanceInfo.enabledLayerCount = uint32_t(validationLayers.size());
        instanceInfo.ppEnabledLayerNames = validationLayers.data();
#else 
        instanceInfo.enabledLayerCount = 0;
#endif

    ThrowIfFailed(vkCreateInstance(&instanceInfo, nullptr, &instance));

#if RAEKOR_DEBUG
    ThrowIfFailed(CreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &debugMessenger));
#endif

    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        throw std::runtime_error("Failed to create vulkan surface");
    }

}



Instance::~Instance() {
#if RAEKOR_DEBUG
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}



PhysicalDevice::PhysicalDevice(const Instance& instance) : 
    gpu(VK_NULL_HANDLE)
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    assert(device_count > 0);

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);
        
        // prefer dedicated GPU
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            gpu = device;
        }
    }

    // else we just get the first adapter found
    if (gpu == VK_NULL_HANDLE) {
        gpu = devices[0];
    }

    VkPhysicalDeviceProperties2 props2 = {};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &properties.rayTracingPipelineProperties;
    properties.rayTracingPipelineProperties.pNext = &properties.descriptorIndexingProperties;

    vkGetPhysicalDeviceProperties2(gpu, &props2);

    limits = props2.properties.limits;
}



VkFormat PhysicalDevice::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
}
    throw std::runtime_error("Unable to find a supported format.");
    return {};
};

} // raekor