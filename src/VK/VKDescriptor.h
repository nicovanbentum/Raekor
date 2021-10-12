#pragma once

#include "VKTexture.h"
#include "VKShader.h"

namespace Raekor::VK {

class BindlessDescriptorSet {
public:
    void create(const Device& device, VkDescriptorType type);
    void destroy(const Device& device);

    uint32_t push_back(const Device& device, const VkDescriptorImageInfo& imageInfo);

    const VkDescriptorSet& getDescriptorSet() const { return set; }
    const VkDescriptorSetLayout& getLayout() const { return layout; }

private:
    uint32_t size = 0;
    std::vector<uint32_t> freeIndices;
    
private:
    VkDescriptorSet set;
    VkDescriptorPool pool;
    VkDescriptorType type;
    VkDescriptorSetLayout layout;

};

} // Raekor::VK