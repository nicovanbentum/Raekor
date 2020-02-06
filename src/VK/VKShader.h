#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Shader {
public:
    Shader(const Context& context, const std::string& path = "");
    ~Shader();
    void reload();
    static void Compile(const char* in, const char* out);
    operator VkShaderModule() { return module; }
    VkPipelineShaderStageCreateInfo getInfo(VkShaderStageFlagBits stage) const;

private:
    VkDevice device;
    std::string filepath;
    VkShaderModule module;

    std::vector<char> readShaderFile(const std::string& path);
};


} // VK
} // Raekor