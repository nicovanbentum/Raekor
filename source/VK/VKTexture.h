#pragma once

#include "Raekor/util.h"

namespace Raekor::VK {

class Texture {
    friend class Device;
    friend class CommandList;

public:
    struct Desc {
        VkImageType type = VK_IMAGE_TYPE_2D;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width = 1, height = 1, depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        bool shaderAccess = false;
        bool mappable = false;
    };

    VkImage image;
    Desc description;

private:
    VmaAllocation allocation;
    std::unordered_map<uint32_t, VkImageView> viewsByMip;
};



class Sampler {
    friend class Device;

public:
    struct Desc {
        VkFilter minFilter = VK_FILTER_NEAREST; 
        VkFilter magFilter = VK_FILTER_NEAREST;
        VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        float anisotropy = 0.0f;
        float maxMipmap = 1.0f;
    };

    VkSampler native() { return sampler; }

private:
    VkSampler sampler;
    Desc description;

};

} // Raekor::VK