#pragma once

#include "util.h"
#include "buffer.h"
#include "VKContext.h"
#include "VKTexture.h"

namespace Raekor::VK {

struct VKMesh {
    uint32_t index;
    uint32_t indexOffset, indexRange, vertexOffset;
    uint32_t textureIndex;
};

class VKScene {
public:
    void load(Context& context);

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

