#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class DescriptorSet {
public:
    DescriptorSet(const Context& ctx) {
        device = ctx.device;
    }
    operator VkDescriptorSet() { return descriptorSet; }

private:
    VkDevice device;
    VkDescriptorSet descriptorSet;
};

class Renderer {
public:
    Renderer(const Context& ctx);
};

} // VK
} // Raekor