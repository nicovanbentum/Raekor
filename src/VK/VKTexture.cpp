#include "pch.h"
#include "VKTexture.h"

namespace Raekor {
namespace VK {

void Image::destroy(VmaAllocator allocator) {
    if(view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
    if(image) vmaDestroyImage(allocator, image, alloc);
    if(sampler) vkDestroySampler(device, sampler, nullptr);
}

Texture::Texture(const Context& ctx, const Stb::Image& image, VmaAllocator allocator) : Image(ctx.device) {
    this->upload(ctx, image, allocator);
}

void Texture::upload(const Context& ctx, const Stb::Image& stb, VmaAllocator allocator) {
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(stb.w, stb.h)))) + 1;

    VkDeviceSize byteSize = stb.w * stb.h * static_cast<uint32_t>(stb.format);

    auto [stagingBuffer, stagingAlloc, stagingAllocInfo] = ctx.device.createStagingBuffer(allocator, byteSize);

    memcpy(stagingAllocInfo.pMappedData, stb.pixels, static_cast<size_t>(byteSize));

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = stb.w;
    imageInfo.extent.height = stb.h;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo imageAllocCreateInfo = {};
    imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    
    auto vkresult = vmaCreateImage(allocator, &imageInfo, &imageAllocCreateInfo, &image, &alloc, &allocInfo);
    assert(vkresult == VK_SUCCESS);
    
    ctx.device.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ctx.device.copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(stb.w), static_cast<uint32_t>(stb.h));
    ctx.device.generateMipmaps(image, stb.w, stb.h, mipLevels);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);

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

CubeTexture::CubeTexture(const Context& ctx, const std::array<Stb::Image, 6>& images, VmaAllocator allocator) : Image(ctx.device) {
    //Calculate the image size and the layer size.
    uint32_t width = images[0].w, height = images[0].h;
    const VkDeviceSize imageSize = width * height * 4 * 6;
    const VkDeviceSize layerSize = imageSize / 6;

    auto [stagingBuffer, stagingAlloc, stagingAllocInfo] = ctx.device.createStagingBuffer(allocator, imageSize);

    for (unsigned int i = 0; i < 6; i++) {
        unsigned char* location = static_cast<unsigned char*>(stagingAllocInfo.pMappedData) + (layerSize * i);
        memcpy(location, images[i].pixels, layerSize);
    }

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

    VmaAllocationCreateInfo imageAllocCreateInfo = {};
    imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto vkresult = vmaCreateImage(allocator, &imageInfo, &imageAllocCreateInfo, &image, &alloc, &allocInfo);
    assert(vkresult == VK_SUCCESS);

    constexpr uint32_t mipLevels = 1;
    constexpr uint32_t layerCount = 6;
    ctx.device.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, layerCount, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ctx.device.copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6);
    ctx.device.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, layerCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);
    
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

    if (vkCreateSampler(ctx.device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk sampler");
    }

    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor.imageView = view;
    descriptor.sampler = sampler;
}

DepthTexture::DepthTexture(const Context& ctx, glm::ivec2 extent, VmaAllocator allocator)
    : Image(ctx.device)
{
    VkFormat depthFormat = ctx.PDevice.findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL, 
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.x;
    imageInfo.extent.height = extent.y;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo imageAllocCreateInfo = {};
    imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto vkresult = vmaCreateImage(allocator, &imageInfo, &imageAllocCreateInfo, &image, &alloc, &allocInfo);
    assert(vkresult == VK_SUCCESS);

    view = ctx.device.createImageView(image, depthFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1);
    ctx.device.transitionImageLayout(image, depthFormat, 1, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

}
}