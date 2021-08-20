#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Shader {
    friend class DescriptorSet;
public:
    
    void create(Device& device, const std::string& filepath) {
        this->filepath = filepath;
        compile(device);
    }

    void destroy(Device& device) {
        vkDestroyShaderModule(device, module, nullptr);
    }

    VkShaderModule getModule() { return module; }
    VkShaderStageFlagBits getStage() { return stage; }

    const std::vector<uint32_t>& getSpirv() {
        return spirv;
    }

    void compile(Device& device);
    static bool compileFromCommandLine(std::string_view in, std::string_view out);
    VkPipelineShaderStageCreateInfo getInfo(VkShaderStageFlagBits stage) const;

private:
    std::string filepath;
    VkShaderModule module = VK_NULL_HANDLE;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirv;

    std::vector<uint32_t> readSpirvFile(const std::string& path);
};

} // VK
} // Raekor