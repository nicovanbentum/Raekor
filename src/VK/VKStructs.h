#pragma once

#include "pch.h"

namespace Raekor::VK {

struct Buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct Pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

}
