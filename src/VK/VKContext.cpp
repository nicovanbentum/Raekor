#include "pch.h"
#include "VKContext.h"
#include "VKDevice.h"

namespace Raekor {
namespace VK {

Context::Context(SDL_Window* window):   
    window(window), 
    instance(window),
    physicalDevice(instance),
    device(instance, physicalDevice) 
{

}

} // VK
} // Raekor