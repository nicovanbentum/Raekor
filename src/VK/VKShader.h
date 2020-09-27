#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Shader {
public:
    // constructors & desctructor
    Shader() = default;
    Shader(const Context& context, const std::string& path = "");
    ~Shader();


    void reload();
    static void compileFromCommandLine(std::string_view in, std::string_view out);
    VkPipelineShaderStageCreateInfo getInfo(VkShaderStageFlagBits stage) const;
    operator VkShaderModule() { return module; }

private:
    VkDevice device;
    std::string filepath;
    VkShaderModule module;

    std::vector<char> readSpirvFile(const std::string& path);
};


} // VK
} // Raekor