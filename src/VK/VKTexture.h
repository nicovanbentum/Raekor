#pragma once

#include "util.h"
#include "VKContext.h"

namespace Raekor {
namespace VK {

class Image {
public:
    void destroy(VmaAllocator allocator);

protected:
    Image(VkDevice device) : device(device) {}
    VkDevice device;

public:
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;

    VmaAllocationInfo allocInfo = {};
    VkDescriptorImageInfo descriptor = {};

    // optional TODO: move somewhere else?
    VkSampler sampler = VK_NULL_HANDLE;
};

class Texture : public Image {
public:
    Texture(const Context& ctx, const Stb::Image& image, VmaAllocator allocator);

private:
    void upload(const Context& ctx, const Stb::Image& image, VmaAllocator allocator);
};

class DepthTexture : public Image {
public:
    DepthTexture(const Context& ctx, glm::ivec2 extent, VmaAllocator allocator);
};

class CubeTexture : public Image {
public:
    CubeTexture(const Context& ctx, const std::array<Stb::Image, 6>& images, VmaAllocator allocator);
};

}
}