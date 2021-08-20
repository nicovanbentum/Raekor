#pragma once

#include "VKBase.h"
#include "VKDevice.h"

namespace Raekor {
namespace VK {

class Context {
public:
    Context(SDL_Window* window);

public:
    SDL_Window* window;
    Instance instance;
    PhysicalDevice physicalDevice;
    Device device;
};

} // VK
} // Raekor