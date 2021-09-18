#pragma once

#include "VKTexture.h"

namespace Raekor::VK {

class Device;

struct VKMesh {
    uint32_t index;
    uint32_t indexOffset, indexRange, vertexOffset;
    uint32_t textureIndex;
};

class VKScene {
public:
    void load(Device& device);

private:
    std::vector<VKMesh> meshes;
    
    std::vector<Texture> textures;

    VkBuffer vertexBuffer;
    VmaAllocation vertexBufferAlloc;
    VmaAllocationInfo vertexBufferAllocInfo;

    VkBuffer indexBuffer;
    VmaAllocation indexBufferAlloc;
    VmaAllocationInfo indexBufferAllocInfo;
};

} // raekor

