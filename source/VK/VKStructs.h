#pragma once

#include "rtti.h"

namespace Raekor::VK {

struct Buffer {
    RTTI_CLASS_HEADER(Buffer);

    VkBuffer buffer;
    VmaAllocation allocation;
};


struct AccelStruct {
    RTTI_CLASS_HEADER(AccelStruct);

    Buffer buffer = {};
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
};


struct RTGeometry {
    RTTI_CLASS_HEADER(RTGeometry);

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


RTTI_CLASS_CPP_INLINE(Buffer) {}
RTTI_CLASS_CPP_INLINE(RTGeometry) {}
RTTI_CLASS_CPP_INLINE(AccelStruct) {}

}
