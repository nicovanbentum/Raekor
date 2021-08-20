#pragma once

#include "VKShader.h"
#include "VKDescriptor.h"

namespace Raekor::VK {

class PathTracePass {
public:
    void initialize(Context& context);
    void destroy(Device& device);

    void createPipeline(Device& device);
    void createShaderBindingTable(Device& device, PhysicalDevice& physicalDevice);

private:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkDescriptorSetLayout descriptorSetLayout; // TODO: move this somewhere else

    Shader rgenShader;
    Shader rmissShader;
    Shader rchitShader;

    VkBuffer shaderBindingTableBuffer;
    VmaAllocation shaderBindingTableAllocation;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
};

} // raekor
