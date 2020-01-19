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
    // no destructor since all members are trivial to destruct
    operator VkPhysicalDevice() { return gpu; }
    operator VkPhysicalDevice() const { return gpu; }
private:
    VkPhysicalDevice gpu;
};

class Device {
    struct Queues {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;
        bool isComplete() { return graphics.has_value() and present.has_value(); }
    };

public:
    Device(const Instance& instance, const PhysicalDevice& physicalDevice);
    ~Device();
    operator VkDevice() { return device; }

private:
    Queues qindices;
    VkDevice device;
    VkQueue presentQueue;
    VkQueue graphicsQueue;
    VkCommandPool commandPool;
};

class Context {
public:
    Context(SDL_Window* window);

public:
    Instance instance;
    PhysicalDevice physicalDevice;
    Device device;
};



} // VK
} // raekor