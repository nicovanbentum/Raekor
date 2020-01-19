#include "pch.h"
#include "VKDevice.h"

namespace Raekor {
namespace VK {

Instance::Instance(SDL_Window* window) {
#ifdef NDEBUG
    isDebug = false;
#else
    isDebug = true;
#endif

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Render Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
    appInfo.pEngineName = "Raekor Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &appInfo;

    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr)) {
        throw std::runtime_error("failed to get vulkan instance extensions");
    }

    std::vector<const char*> extensions = { VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
    size_t additional_extension_count = extensions.size();
    extensions.resize(additional_extension_count + count);

    auto sdl_bool = SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data() + additional_extension_count);
    if (!sdl_bool) {
        throw std::runtime_error("failed to get instance extensions");
    }

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    if (isDebug) {
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
            if (!found) throw std::runtime_error("requested validation layer not supported");
        }
    }

    // Now we can make the Vulkan instance
    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();

    if (isDebug) {
        instance_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instance_info.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        instance_info.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vulkan instance");
    }

    if (SDL_Vulkan_CreateSurface(window, instance, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vulkan surface");
    }
}

PhysicalDevice::PhysicalDevice(const Instance& instance)
    : gpu(VK_NULL_HANDLE)
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0) {
        throw std::runtime_error("failed to find any physical devices");
    }

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
    // TODO: implement actual device picking and scoring
    if (gpu == VK_NULL_HANDLE) {
        gpu = *devices.begin();
    }
}



Device::Device(const Instance& instance, const PhysicalDevice& GPU) {
#ifdef NDEBUG
    bool isDebug = false;
#else
    bool isDebug = true;
#endif
    const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(GPU, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(GPU, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(GPU, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(GPU, &queueFamilyCount, queueFamilies.data());

    int queue_index = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            qindices.graphics = queue_index;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(GPU, queue_index, instance.getSurface(), &presentSupport);
        if (queueFamily.queueCount > 0 && presentSupport) {
            qindices.present = queue_index;
        }
        queue_index++;
    }
    if (!qindices.isComplete() || !requiredExtensions.empty()) {
        throw std::runtime_error("queue family and/or extensions failed");
    }
    
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = qindices.graphics.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { qindices.graphics.value(), qindices.present.value() };

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

    // TODO: abstract debug
    const std::vector<const char*> validationLayers = {
    "VK_LAYER_LUNARG_standard_validation"
    };

    if (isDebug) {
        device_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        device_info.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        device_info.enabledLayerCount = 0;
    }

    if (vkCreateDevice(GPU, &device_info, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk logical device");
    }

    vkGetDeviceQueue(device, qindices.graphics.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, qindices.present.value(), 0, &presentQueue);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = qindices.graphics.value();
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk command pool");
    }
}

Context::Context(SDL_Window* window) 
    :   instance(window), 
        physicalDevice(instance), 
        device(instance, physicalDevice)
{

}

Instance::~Instance() {
    vkDestroyInstance(instance, nullptr);
}

Device::~Device() {
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
}


} // VK
} // raekor