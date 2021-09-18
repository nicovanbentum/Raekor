#pragma once

namespace Raekor::VK {

class Device;

class Shader {
    friend class DescriptorSet;

public:
    static bool glslangValidator(const char* vulkanSDK, const fs::directory_entry& file);
    
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