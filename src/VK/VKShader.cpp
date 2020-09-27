#include "pch.h"
#include "VKShader.h"

namespace Raekor {
namespace VK {

Shader::Shader(const Context& ctx, const std::string& path) :
    device(ctx.device), 
    filepath(path), 
    module(VK_NULL_HANDLE)
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

    const auto buffer = readSpirvFile(filepath);
    
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
    
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk shader module");
    }
}

///////////////////////////////////////////////////////////////////////////

void Shader::compileFromCommandLine(std::string_view in, std::string_view out) {
    const auto vulkan_sdk_path = getenv("VULKAN_SDK");
    if (!vulkan_sdk_path) {
        std::puts("Unable to find Vulkan SDK, cannot compile shader");
        return;
    }

    const auto compiler = vulkan_sdk_path + std::string("\\Bin\\glslc.exe ");
    const auto command = compiler + std::string(in) + " -o " + std::string(out);

    if (system(command.c_str()) != 0) {
        std::cout << "failed to compile vulkan shader: " << in << '\n';
    } else {
        std::cout << "Successfully compiled VK shader: " << in << '\n';
    }
}

///////////////////////////////////////////////////////////////////////////

std::vector<char> Shader::readSpirvFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    
    const size_t filesize = static_cast<size_t>(file.tellg());
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