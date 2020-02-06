#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Renderer {
public:
    Renderer(const Context& ctx);
    void render() const {

    }
};

class FrameBuffer {
public:
    FrameBuffer(const Context& ctx, glm::ivec2 size)
        : device(ctx.device) {
        extent = { 
            static_cast<uint32_t>(size.x), 
            static_cast<uint32_t>(size.y) 
        };
    }

    ~FrameBuffer() {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    operator VkFramebuffer() const { return framebuffer; }

private:
    VkDevice device;
    VkExtent2D extent;
    VkFramebuffer framebuffer;
};

} // VK
} // Raekor