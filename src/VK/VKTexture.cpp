#include "pch.h"
#include "VKTexture.h"

#include "VKDevice.h"
#include "VKUtil.h"

namespace Raekor::VK {

Texture Device::createTexture(const Texture::Desc& desc) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = desc.format;
    imageInfo.extent = { desc.width, desc.height, desc.depth };
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = desc.arrayLayers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (desc.isFramebufferAttachment) {
        if (desc.format >= VK_FORMAT_D16_UNORM &&
            desc.format <= VK_FORMAT_D32_SFLOAT_S8_UINT) {
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else {
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    Texture texture;
    texture.description = desc;

    ThrowIfFailed(vmaCreateImage(allocator, &imageInfo, &allocInfo, &texture.image, &texture.allocation, nullptr));
        
    return texture;
}
    


void Device::destroyTexture(Texture& texture) {
    for (const auto& [mip, view] : texture.viewsByMip) {
        vkDestroyImageView(device, view, nullptr);
    }

    vmaDestroyImage(allocator, texture.image, texture.allocation);
}



VkImageView Device::createView(Texture& texture, uint32_t mipLevel) {
    const auto& desc = texture.description;
    auto& views = texture.viewsByMip;
        
    if (views.find(mipLevel) != views.end()) {
        return views.at(mipLevel);
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = desc.format;
    viewInfo.image = texture.image;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.baseMipLevel = mipLevel;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    if (desc.format >= VK_FORMAT_D16_UNORM &&
        desc.format <= VK_FORMAT_D32_SFLOAT_S8_UINT) {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageView view;
    vkCreateImageView(device, &viewInfo, nullptr, &view);

    views.insert({ mipLevel, view });
    
    return view;
}



Sampler Device::createSampler(const Sampler::Desc& desc) {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = desc.minFilter;
    samplerInfo.minFilter = desc.magFilter;
    samplerInfo.addressModeU = desc.addressMode;
    samplerInfo.addressModeV = desc.addressMode;
    samplerInfo.addressModeW = desc.addressMode;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor = desc.borderColor;

    if (desc.anisotropy) {
        samplerInfo.maxAnisotropy = desc.anisotropy;
        samplerInfo.anisotropyEnable = VK_TRUE;
    }


    if (desc.maxMipmap) {
        samplerInfo.maxLod = desc.maxMipmap;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod = desc.maxMipmap;

    Sampler sampler;
    sampler.description = desc;
    
    vkCreateSampler(device, &samplerInfo, nullptr, &sampler.sampler);

    return sampler;
}



void Device::destroySampler(Sampler& sampler) {
    vkDestroySampler(device, sampler.sampler, nullptr);
}

} // Raekor::VK