#include "pch.h"
#include "VKDescriptor.h"

namespace Raekor {
namespace VK {

UniformBuffer::UniformBuffer(const Context& ctx, VmaAllocator allocator, size_t size) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    auto vkresult = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &alloc, &allocationInfo);
    assert(vkresult == VK_SUCCESS);

    descriptor.buffer = buffer;
    descriptor.offset = 0;
    descriptor.range = VK_WHOLE_SIZE;
}

///////////////////////////////////////////////////////////////////////

void UniformBuffer::update(VmaAllocator allocator, const void* data, size_t size) const {
    size = size == VK_WHOLE_SIZE ? allocationInfo.size : size;
    memcpy(allocationInfo.pMappedData, data, size);
    vmaFlushAllocation(allocator, alloc, 0, size);
}

///////////////////////////////////////////////////////////////////////

void UniformBuffer::destroy(VmaAllocator allocator) {
    vmaDestroyBuffer(allocator, buffer, alloc);
}

///////////////////////////////////////////////////////////////////////

DescriptorSet::DescriptorSet(Context& context, Shader** shaders, size_t count) {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (size_t i = 0; i < count; i++) {
        Shader* shader = shaders[i];

        auto compiler = spirv_cross::CompilerGLSL(shader->spirv.data(), shader->spirv.size() / sizeof(uint32_t));
        auto resources = compiler.get_shader_resources();

        for (auto& resource : resources.sampled_images) {
            bindings.push_back(getBinding(&compiler, resource, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER));
        }

        for (auto& resource : resources.uniform_buffers) {
            bindings.push_back(getBinding(&compiler, resource, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC));
        }
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    for (auto& binding : bindings) {
        std::cout << " completed binding at " << binding.binding << '\n';
    }

    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("fialed to create descriptor layout");
    }

    context.device.allocateDescriptorSet(1, &descriptorSetLayout, &descriptorSet);

    for (size_t i = 0; i < count; i++) {
        Shader* shader = shaders[i];

        auto compiler = spirv_cross::CompilerGLSL(shader->spirv.data(), shader->spirv.size() / sizeof(uint32_t));
        auto resources = compiler.get_shader_resources();

        for (auto& resource : resources.sampled_images) {
            this->resources[resource.name] = getDescriptorSet(&compiler, resource, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }

        for (auto& resource : resources.uniform_buffers) {
            this->resources[resource.name] = getDescriptorSet(&compiler, resource, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        }
    }

    for (auto& res : this->resources) {
        std::cout << "Completed descriptor at " << res.second.dstBinding << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::destroy(const Device& device) {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    device.freeDescriptorSet(1, &descriptorSet);
}

///////////////////////////////////////////////////////////////////////

DescriptorSet::operator VkDescriptorSet() { return descriptorSet; }

///////////////////////////////////////////////////////////////////////

VkDescriptorSetLayout& DescriptorSet::getLayout() { return descriptorSetLayout; }

///////////////////////////////////////////////////////////////////////

VkWriteDescriptorSet* DescriptorSet::getResource(const std::string& name) {
    auto it = resources.find(name);
    if (it != resources.end()) {
        return &it->second;
    }
    else {
        return nullptr;
    }
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::update(VkDevice device) {
    std::vector<VkWriteDescriptorSet> writeSets;
    std::for_each(resources.begin(), resources.end(), [&](const auto& kv) {
        writeSets.push_back(kv.second);
        });

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::update(VkDevice device, const std::string& name) {
    auto resource = getResource(name);
    if (resource) {
        vkUpdateDescriptorSets(device, 1, getResource(name), 0, nullptr);
    }
}

///////////////////////////////////////////////////////////////////////

VkDescriptorSetLayoutBinding DescriptorSet::getBinding(spirv_cross::Compiler* compiler, spirv_cross::Resource& resource, VkDescriptorType descriptorType) {
    auto& type = compiler->get_type(resource.type_id);

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
    binding.descriptorType = descriptorType;
    binding.descriptorCount = type.array.size() ? type.array[0] : 1;
    binding.stageFlags = getStageFromExecutionModel(compiler->get_execution_model());
    binding.pImmutableSamplers = nullptr;

    return binding;
}

///////////////////////////////////////////////////////////////////////

VkWriteDescriptorSet DescriptorSet::getDescriptorSet(spirv_cross::Compiler* compiler, spirv_cross::Resource& resource, VkDescriptorType descriptorType) {
    auto& type = compiler->get_type(resource.type_id);

    VkWriteDescriptorSet descriptor = {};
    descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor.dstSet = descriptorSet;
    descriptor.dstBinding = compiler->get_decoration(resource.id, spv::DecorationBinding);
    descriptor.dstArrayElement = 0;
    descriptor.descriptorType = descriptorType;
    descriptor.descriptorCount = type.array.size() ? type.array[0] : 1;
    descriptor.pBufferInfo = nullptr;
    descriptor.pImageInfo = nullptr;
    descriptor.pTexelBufferView = nullptr;

    return descriptor;
}

} // VK
} // Raekor