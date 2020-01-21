#include "pch.h"
#include "VKTexture.h"

namespace Raekor {
namespace VK {

Texture::Texture(const Context& ctx, const Stb::Image& image) : device(ctx.device) {
    this->upload(ctx, image); 
}

Texture::~Texture() {
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

void Texture::upload(const Context& ctx, const Stb::Image& stb) {
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(stb.w, stb.h)))) + 1;

    VkDeviceSize image_size = stb.w * stb.h * static_cast<uint32_t>(stb.format);
    VkBuffer stage_pixels;
    VkDeviceMemory stage_pixels_mem;

    ctx.device.createBuffer(
        image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stage_pixels,
        stage_pixels_mem
    );

    void* pdata;
    vkMapMemory(device, stage_pixels_mem, 0, image_size, 0, &pdata);
    memcpy(pdata, stb.pixels, static_cast<size_t>(image_size));
    vkUnmapMemory(device, stage_pixels_mem);

    ctx.device.createImage(
        stb.w, stb.h, mipLevels, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory
    );
    ctx.device.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ctx.device.copyBufferToImage(stage_pixels, image, static_cast<uint32_t>(stb.w), static_cast<uint32_t>(stb.h));
    ctx.device.generateMipmaps(image, stb.w, stb.h, mipLevels);
    
    vkDestroyBuffer(device, stage_pixels, nullptr);
    vkFreeMemory(device, stage_pixels_mem, nullptr);

    view = ctx.device.createImageView(image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 1);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);

    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk sampler");
    }

    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor.imageView = view;
    descriptor.sampler = sampler;
}

DepthTexture::DepthTexture(const Context & ctx, glm::ivec2 extent)
    : device(ctx.device) 
{
    VkFormat depthFormat = ctx.PDevice.findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    ctx.device.createImage(
        static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y),
        1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        image, memory);

    view = ctx.device.createImageView(image, depthFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1);
    ctx.device.transitionImageLayout(image, depthFormat, 1, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

DepthTexture::~DepthTexture() {
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

}
}