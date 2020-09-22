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
    static void compileFromCommandLine(const std::string& originalFileName, const std::string& outFilename);
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