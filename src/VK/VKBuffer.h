#pragma once

#include "pch.h"
#include "buffer.h"
#include "VKContext.h"

namespace Raekor {
namespace VK {

class Buffer {
protected:
    Buffer(VkDevice device);
    ~Buffer();

public:
    inline VkBuffer getBuffer() const { return buffer; }
    inline VkDeviceMemory getMemory() const { return memory; }
    static VkFormat toVkFormat(ShaderType type);

protected:
    VkDevice device;
    VkBuffer buffer;
    VkDeviceMemory memory;
};

///////////////////////////////////////////////////////////////////

class IndexBuffer : public Buffer {
public:
    IndexBuffer(const Context& ctx, const std::vector<Index>& indices);
    inline uint32_t getCount() const { return count; }

private:
    uint32_t count;
};

///////////////////////////////////////////////////////////////////

class VertexBuffer : public Buffer {
public:
    VertexBuffer(const Context& ctx, std::vector<Vertex>& vertices);
    void describe();
    void setLayout(const InputLayout& new_layout);
    VkPipelineVertexInputStateCreateInfo getInfo();

    inline VkPipelineVertexInputStateCreateInfo getVertexInputState() { return info; }

private:
    InputLayout extLayout;
    VkPipelineVertexInputStateCreateInfo info;
    VkVertexInputBindingDescription bindingDescription;
    std::vector<VkVertexInputAttributeDescription> layout;
};

}
}