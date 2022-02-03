#pragma once

namespace Raekor::VK {

struct Buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AccelerationStructure {
    Buffer buffer = {};
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
};

struct RTGeometry {
    Buffer vertices;
    Buffer indices;
    AccelerationStructure accelStruct;
};

struct RTMaterial {
    glm::vec4 albedo;
    glm::ivec4 textures; // x = albedo, y = normals, z = metalrough
    glm::vec4 properties; // x = metalness, y = roughness, z = emissive
};

}
