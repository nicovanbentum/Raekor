#pragma once

namespace Raekor::VK {

class Instance {
    friend class Device;

public:
    Instance() = delete;
    Instance(Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&) = delete;
    Instance& operator=(Instance&&) = delete;

    explicit Instance(SDL_Window* window);
    ~Instance();

    operator VkInstance() const { return instance; }
    
    inline VkSurfaceKHR getSurface() const { return surface; }

private:
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
};



class PhysicalDevice {
    friend class Device;

private:
    PhysicalDevice() = delete;
    PhysicalDevice(PhysicalDevice&) = delete;
    PhysicalDevice(PhysicalDevice&&) = delete;
    PhysicalDevice& operator=(PhysicalDevice&) = delete;
    PhysicalDevice& operator=(PhysicalDevice&&) = delete;

public:
    explicit PhysicalDevice(const Instance& instance);
    operator VkPhysicalDevice() const { return gpu; }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

private:
    struct Properties {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties { 
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR 
        };
        VkPhysicalDeviceDescriptorIndexingProperties descriptorIndexingProperties = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES
        };

    } properties;

    VkPhysicalDeviceLimits limits;

    VkPhysicalDevice gpu = VK_NULL_HANDLE;
};

} // raekor