#pragma once

#include "util.h"
#include "VKContext.h"

namespace Raekor {
namespace VK {

class Texture {
public:
    Texture(const Context& ctx, const Stb::Image& image);
    ~Texture();

    inline VkImage getImage() const { return image; }
    inline VkImageView getView() const { return view; }
    inline VkSampler getSampler() const { return sampler; }

    inline const VkDescriptorImageInfo* getDescriptor() const { return &descriptor; }

private:
    void upload(const Context& ctx, const Stb::Image& image);

private:
    VkImage image;
    VkDevice device;
    VkImageView view;
    VkSampler sampler;
    VkDeviceMemory memory;
    VkDescriptorImageInfo descriptor;
};

class DepthTexture {
public:
    DepthTexture(const Context& ctx, glm::ivec2 extent);
    ~DepthTexture();

    inline VkImage getImage() const { return image; }
    inline VkImageView getView() const { return view; }

    inline const VkDescriptorImageInfo* getDescriptor() const { return &descriptor; }

private:
    VkImage image;
    VkDevice device;
    VkImageView view;
    VkDeviceMemory memory;
    VkDescriptorImageInfo descriptor;

};

}
}