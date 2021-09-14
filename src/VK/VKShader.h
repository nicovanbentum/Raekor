#pragma once

#include "VKContext.h"

namespace Raekor::VK {

class Shader {
    friend class DescriptorSet;
public:
    static bool compileFromCommandLine(const fs::path& InShader, const fs::path& OutBinary);
    
    void create(Device& device, const std::string& filepath);
    void destroy(Device& device);

    VkShaderModule getModule() const { return module; }
    VkShaderStageFlagBits getStage() const { return stage; }
    VkPipelineShaderStageCreateInfo getPipelineCreateInfo() const;

private:
    std::string filepath;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirv;
    VkShaderModule module = VK_NULL_HANDLE;
};

} // Raekor::VK