#pragma once

namespace Raekor::VK {

struct Buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AccelStruct {
    Buffer buffer = {};
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
};

struct RTGeometry {
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

}
