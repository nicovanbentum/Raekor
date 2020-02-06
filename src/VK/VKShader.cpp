#include "pch.h"
#include "VKShader.h"

#define _glslc "dependencies\\glslc.exe "
#define _cl(in, out) system(std::string(_glslc + static_cast<std::string>(in) + static_cast<std::string>(" -o ") + static_cast<std::string>(out)).c_str())

namespace Raekor {
namespace VK {

Shader::Shader(const Context& ctx, const std::string& path) 
    : device(ctx.device), filepath(path), module(VK_NULL_HANDLE)
{
    if (filepath.empty()) return;
    reload();
}

///////////////////////////////////////////////////////////////////////////

Shader::~Shader() {
    vkDestroyShaderModule(device, module, nullptr);
}

///////////////////////////////////////////////////////////////////////////

void Shader::reload() {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, module, nullptr);
    }
    auto buffer = readShaderFile(filepath);
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk shader module");
    }
}

///////////////////////////////////////////////////////////////////////////

void Shader::Compile(const char* in, const char* out) {
    if (_cl(in, out) != 0) {
        std::cout << "failed to compile vulkan shader: " + std::string(in) << '\n';
    } else {
        std::cout << "Successfully compiled VK shader: " + std::string(in) << '\n';
    }
}

///////////////////////////////////////////////////////////////////////////

std::vector<char> Shader::readShaderFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("failed to open " + path).c_str());
    }
    size_t filesize = (size_t)file.tellg();
    std::vector<char> buffer(filesize);
    file.seekg(0);
    file.read(buffer.data(), filesize);
    file.close();
    return buffer;
}

///////////////////////////////////////////////////////////////////////////

VkPipelineShaderStageCreateInfo Shader::getInfo(VkShaderStageFlagBits stage) const {
    VkPipelineShaderStageCreateInfo stage_info = {};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = stage;
    stage_info.module = module;
    stage_info.pName = "main";
    return stage_info;
}

} // VK
} // Raekor