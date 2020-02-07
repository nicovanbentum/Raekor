#pragma once

#include "util.h"
#include "VKContext.h"

namespace Raekor {
namespace VK {

class Image {
public:
    ~Image();

protected:
    Image(VkDevice device) : device(device) {}
    VkDevice device;

public:
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
    std::optional<VkSampler> sampler;
    VkDescriptorImageInfo descriptor;

};

class Texture : public Image {
public:
    Texture(const Context& ctx, const Stb::Image& image);

private:
    void upload(const Context& ctx, const Stb::Image& image);
};

class DepthTexture : public Image {
public:
    DepthTexture(const Context& ctx, glm::ivec2 extent);
};

class CubeTexture : public Image {
public:
    CubeTexture(const Context& ctx, const std::array<Stb::Image, 6>& images);
};

}
}