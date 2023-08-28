#pragma once

#include "rtti.h"

namespace Raekor::VK {

struct Buffer {
    RTTI_DECLARE_TYPE(Buffer);

    VkBuffer buffer;
    VmaAllocation allocation;
};


struct AccelStruct {
    RTTI_DECLARE_TYPE(AccelStruct);

    Buffer buffer = {};
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
};


struct RTGeometry {
    RTTI_DECLARE_TYPE(RTGeometry);

    AccelStruct accelStruct;
    Buffer vertices;
    Buffer indices;
};


struct RTMaterial {
    glm::vec4 albedo;
    glm::ivec4 textures; // texture indices:    x = albedo,    y = normals,   z = metalrough
    glm::vec4 properties; // scalar properties: x = metalness, y = roughness
    glm::vec4 emissive;
};


RTTI_DEFINE_TYPE_INLINE(Buffer) {}
RTTI_DEFINE_TYPE_INLINE(RTGeometry) {}
RTTI_DEFINE_TYPE_INLINE(AccelStruct) {}

}
