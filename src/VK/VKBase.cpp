#include "pch.h"
#include "VKBase.h"

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << pCallbackData->pMessage << '\n';

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        assert(false);
    }

    return VK_FALSE;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
        "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
        "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } 	else 	{
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

namespace Raekor {
namespace VK {

Instance::Instance(SDL_Window* window) {
    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "Raekor Editor";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 2, 0);
    appInfo.pEngineName = "Raekor";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instance_info.pApplicationInfo = &appInfo;

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

    auto sdlBool = SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data() + additionalExtensionCount);
    if (!sdlBool) {
        throw std::runtime_error("failed to get instance extensions");
    }

    for (auto extension : extensions) {
        std::cout << extension << '\n';
    }

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };


#if RAEKOR_DEBUG
        VkDebugUtilsMessengerCreateInfoEXT debugInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = debugCallback;

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

        instance_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugInfo;
#endif

    // Now we can make the Vulkan instance
    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();

    if (RAEKOR_DEBUG) {
        instance_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instance_info.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        instance_info.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vulkan instance");
    }

#if RAEKOR_DEBUG
    CreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &debugMessenger);
#endif

    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        throw std::runtime_error("failed to create vulkan surface");
    }
}

//////////////////////////////////////////////////////////////////////////

Instance::~Instance() {
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

//////////////////////////////////////////////////////////////////////////

PhysicalDevice::PhysicalDevice(const Instance& instance)
    : gpu(VK_NULL_HANDLE)
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
}

//////////////////////////////////////////////////////////////////////////

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
    throw std::runtime_error("unable to find a supported format");
    return {};
};

} // VK
} // raekor