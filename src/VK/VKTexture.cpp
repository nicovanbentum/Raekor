#include "pch.h"
#include "VKTexture.h"

namespace Raekor {
namespace VK {

Image::~Image() {
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    if (sampler.has_value()) {
        vkDestroySampler(device, sampler.value(), nullptr);
    }
    vkFreeMemory(device, memory, nullptr);
}

Texture::Texture(const Context& ctx, const Stb::Image& image) : Image(ctx.device) {
    this->upload(ctx, image);
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

    sampler = VkSampler{};
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler.value()) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk sampler");
    }

    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor.imageView = view;
    descriptor.sampler = sampler.value();
}

CubeTexture::CubeTexture(const Context& ctx, const std::array<Stb::Image, 6>& images) : Image(ctx.device) {
    //Calculate the image size and the layer size.
    uint32_t width = images[0].w, height = images[0].h;
    const VkDeviceSize imageSize = width * height * 4 * 6;
    const VkDeviceSize layerSize = imageSize / 6;

    // setup the stage buffer
    VkBuffer stage_pixels;
    VkDeviceMemory stage_pixels_mem;

    ctx.device.createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stage_pixels,
        stage_pixels_mem
    );

    // map and copy the memory over
    void* pdata;
    vkMapMemory(ctx.device, stage_pixels_mem, 0, imageSize, 0, &pdata);
    for (unsigned int i = 0; i < 6; i++) {
        unsigned char* location = static_cast<unsigned char*>(pdata) + (layerSize * i);
        memcpy(location, images[i].pixels, layerSize);
    }
    vkUnmapMemory(ctx.device, stage_pixels_mem);

    // create the image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(ctx.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk image");
    }

    // allocate and bind GPU memory for us to use
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ctx.device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = ctx.device.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(ctx.device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image meory");
    }

    vkBindImageMemory(ctx.device, image, memory, 0);

    constexpr uint32_t mipLevels = 1;
    constexpr uint32_t layerCount = 6;
    ctx.device.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, layerCount, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ctx.device.copyBufferToImage(stage_pixels, image, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6);
    ctx.device.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, layerCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkDestroyBuffer(ctx.device, stage_pixels, nullptr);
    vkFreeMemory(ctx.device, stage_pixels_mem, nullptr);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(ctx.device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

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
    samplerInfo.maxLod = 1.0f;

    sampler = VkSampler{};
    if (vkCreateSampler(ctx.device, &samplerInfo, nullptr, &sampler.value()) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk sampler");
    }

    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor.imageView = view;
    descriptor.sampler = sampler.value();
}

DepthTexture::DepthTexture(const Context & ctx, glm::ivec2 extent)
    : Image(ctx.device)
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

}
}