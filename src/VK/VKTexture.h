#pragma once

#include "util.h"

namespace Raekor::VK {

class Device;

class Image {
public:
    void destroy(VmaAllocator allocator);

protected:
    Image() = default;
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
    Texture(Device& device, const Stb::Image& image);

private:
    void upload(Device& device, const Stb::Image& image);
};

class DepthTexture : public Image {
public:
    DepthTexture(Device& device, glm::ivec2 extent);
};

class CubeTexture : public Image {
public:
    CubeTexture(Device& device, const std::array<Stb::Image, 6>& images);
};

} // Raekor::VK