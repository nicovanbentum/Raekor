#pragma once

namespace Raekor::VK {

class Device;

class Shader {
    friend class DescriptorSet;
    friend class Device;

public:
    static bool glslangValidator(const char* vulkanSDK, const fs::directory_entry& file);
    static bool DXC(const fs::directory_entry& file);
    
    VkShaderModule getModule() const { return module; }
    VkShaderStageFlagBits getStage() const { return stage; }
    VkPipelineShaderStageCreateInfo getPipelineCreateInfo() const;

private:
    std::string filepath;
    VkShaderStageFlagBits stage;
    VkShaderModule module = VK_NULL_HANDLE;
    SpvReflectShaderModule reflectModule;
};

} // Raekor::VK