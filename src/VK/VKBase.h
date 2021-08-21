#pragma once

namespace Raekor {
namespace VK {

class Instance {
public:
    Instance(SDL_Window* window);
    ~Instance();
    operator VkInstance() { return instance; }
    operator VkInstance() const { return instance; }
    inline VkSurfaceKHR getSurface() const { return surface; }

private:
    VkInstance instance;
    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT debugMessenger;
};


class PhysicalDevice {
    struct Properties {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties { 
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR 
        };
    } properties;

public:
    PhysicalDevice(const Instance& instance);
    operator VkPhysicalDevice() { return gpu; }
    operator VkPhysicalDevice() const { return gpu; }

    const Properties& getProperties() const { return properties; }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

private:
    VkPhysicalDevice gpu;

};

} // VK
} // raekor