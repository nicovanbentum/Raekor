#pragma once

#include "VKBase.h"
#include "VKDevice.h"

namespace Raekor {
namespace VK {

class Context {
public:
    Context(SDL_Window* window);

public:
    Instance instance;
    PhysicalDevice PDevice;
    Device device;
};

} // VK
} // Raekor