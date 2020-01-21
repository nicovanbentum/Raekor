#pragma once

namespace Raekor {
namespace VK {

class Instance {
public:
    Instance(SDL_Window* window);
    ~Instance();
    operator VkInstance() { return instance; }
    operator VkInstance() const { return instance; }
    VkSurfaceKHR getSurface() const { return surface; }

private:
    bool isDebug;
    VkInstance instance;
    VkSurfaceKHR surface;

};


class PhysicalDevice {
public:
    PhysicalDevice(const Instance& instance);
    operator VkPhysicalDevice() { return gpu; }
    operator VkPhysicalDevice() const { return gpu; }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
private:
    VkPhysicalDevice gpu;
};

} // VK
} // raekor