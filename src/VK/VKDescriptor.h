#pragma once

#include "VKTexture.h"
#include "VKShader.h"

namespace Raekor {
namespace VK {

class UniformBuffer {
public:
    UniformBuffer(const Context& ctx, VmaAllocator allocator, size_t allocationSize);

    void update(VmaAllocator allocator, const void* data, size_t size = VK_WHOLE_SIZE) const;
    inline const VkDescriptorBufferInfo* getDescriptor() const { return &descriptor; }
    void destroy(VmaAllocator allocator);

public:
    VkBuffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo allocationInfo;
    VkDescriptorBufferInfo descriptor;
};

///////////////////////////////////////////////////////////////////////

class DescriptorSet {
public:
    DescriptorSet() = default;
    DescriptorSet(Context& context, Shader** shaders, size_t count);
    void destroy(const Device& device);

    operator VkDescriptorSet();
    VkDescriptorSetLayout& getLayout();

    VkWriteDescriptorSet* getResource(const std::string& name);

    void update(VkDevice device);
    void update(VkDevice device, const std::string& name);

private:
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    std::unordered_map<std::string, VkWriteDescriptorSet> resources;
    
private:
    VkWriteDescriptorSet getDescriptorSet(spirv_cross::Compiler* compiler, spirv_cross::Resource& resource, VkDescriptorType descriptorType);
    VkDescriptorSetLayoutBinding getBinding(spirv_cross::Compiler* compiler, spirv_cross::Resource& resource, VkDescriptorType descriptorType);

};

} // VK
} // Raekor