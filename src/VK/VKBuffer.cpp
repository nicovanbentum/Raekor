#include "pch.h"
#include "VKBuffer.h"
#include "VKContext.h"

namespace Raekor {
namespace VK {

Buffer::Buffer(VkDevice device) : device(device) {}

Buffer::~Buffer() {
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

VkFormat Buffer::toVkFormat(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT1: return VK_FORMAT_R32_SFLOAT;
    case ShaderType::FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
    case ShaderType::FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
    case ShaderType::FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
    default: return VK_FORMAT_UNDEFINED;
    }
}

IndexBuffer::IndexBuffer(const Context& ctx, const std::vector<Triangle>& indices) 
    : Buffer(ctx.device), count(static_cast<uint32_t>(indices.size() * 3)) {
    ctx.device.uploadBuffer<Triangle>(indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, buffer, memory);
}

VertexBuffer::VertexBuffer(const Context& ctx, std::vector<Vertex>& vertices) 
    : Buffer(ctx.device), info({}) {
    ctx.device.uploadBuffer<Vertex>(vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, buffer, memory);
    describe();
}

void VertexBuffer::setLayout(const InputLayout& new_layout) {
    extLayout = new_layout;
    layout.clear();
    uint32_t location = 0;
    for (auto& element : new_layout) {
        VkVertexInputAttributeDescription attrib = {};
        attrib.binding = 0;
        attrib.location = location++;
        attrib.format = toVkFormat(element.type);
        attrib.offset = element.offset;
        layout.push_back(attrib);
    }
    info.vertexAttributeDescriptionCount = static_cast<uint32_t>(layout.size());
    info.pVertexAttributeDescriptions = layout.data();
}

void VertexBuffer::describe() {
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.vertexBindingDescriptionCount = 1;
    info.pVertexBindingDescriptions = &bindingDescription;
}

VkPipelineVertexInputStateCreateInfo VertexBuffer::getState() {
    setLayout(extLayout);
    describe();
    return info;
}

VulkanBuffer::VulkanBuffer(VmaAllocator allocator, const VkBufferCreateInfo* pBufferCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo) {
    auto vkresult = vmaCreateBuffer(allocator, pBufferCreateInfo, pAllocationCreateInfo, &buffer, &alloc, &allocInfo);
    assert(vkresult == VK_SUCCESS);
}

VulkanBuffer::Unique VulkanBuffer::create(VmaAllocator allocator, const VkBufferCreateInfo* pBufferCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo) {
    auto vkbuffer = new VulkanBuffer();
    vmaCreateBuffer(allocator, pBufferCreateInfo, pAllocationCreateInfo, &vkbuffer->buffer, &vkbuffer->alloc, &vkbuffer->allocInfo);
    return Unique(vkbuffer, [=](VulkanBuffer* f) {
        assert(allocator);
        f->destroy(allocator);
        });
}

void VulkanBuffer::destroy(VmaAllocator allocator) {
    vmaDestroyBuffer(allocator, buffer, alloc);
    std::cout << "destroyed " << std::endl;
}

}
}