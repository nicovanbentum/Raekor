#pragma once

namespace Raekor::VK {

class Device;

class Shader {
    friend class DescriptorSet;
    friend class Device;

public:
    static bool sCompileGLSL(const char* vulkanSDK, const fs::directory_entry& file);
    static bool sCompileHLSL(const fs::directory_entry& file);
    
    VkShaderModule GetModule() const { return module; }
    VkShaderStageFlagBits GetStage() const { return stage; }
    VkPipelineShaderStageCreateInfo GetPipelineCreateInfo() const;

private:
    std::string filepath;
    VkShaderStageFlagBits stage;
    VkShaderModule module = VK_NULL_HANDLE;
    SpvReflectShaderModule reflectModule;
};

} // Raekor::VK