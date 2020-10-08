#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

    VkShaderStageFlagBits getStageFromExecutionModel(spv::ExecutionModel model);

class Shader {
    friend class DescriptorSet;
public:
    // constructors & desctructor
    Shader() = default;
    Shader(const Context& context, const std::string& path = "");
    ~Shader();

    operator VkShaderModule() { return module; }
    void reload();
    static void compileFromCommandLine(std::string_view in, std::string_view out);
    VkPipelineShaderStageCreateInfo getInfo(VkShaderStageFlagBits stage) const;

private:
    VkDevice device;
    std::string filepath;
    VkShaderModule module;
    std::vector<uint32_t> spirv;

    std::vector<uint32_t> readSpirvFile(const std::string& path);
};


} // VK
} // Raekor