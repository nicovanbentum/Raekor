#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Shader {
    friend class DescriptorSet;
public:
    // constructors & desctructor
    Shader() = default;
    Shader(VkDevice device, const std::string& path = "");
    ~Shader();

    operator VkShaderModule() { return module; }

    VkShaderModule getModule() { return module; }
    VkShaderStageFlagBits getStage() { return stage; }

    const std::vector<uint32_t>& getSpirv() {
        return spirv;
    }

    void reload();
    static bool compileFromCommandLine(std::string_view in, std::string_view out);
    VkPipelineShaderStageCreateInfo getInfo(VkShaderStageFlagBits stage) const;

private:
    VkDevice device;
    std::string filepath;
    VkShaderModule module;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirv;

    std::vector<uint32_t> readSpirvFile(const std::string& path);
};

} // VK
} // Raekor