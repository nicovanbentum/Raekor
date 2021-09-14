#include "pch.h"
#include "VKShader.h"

namespace Raekor::VK {

bool Shader::compileFromCommandLine(const fs::path& inShader, const fs::path& outBinary) {
    if (!fs::exists(inShader)) {
        std::cout << "Shader file " << inShader.string() << " does not exist on disk.\n";
        return false;
    }

    const auto vulkan_sdk_path = getenv("VULKAN_SDK");
    if (!vulkan_sdk_path) {
        std::puts("Unable to find Vulkan SDK, cannot compile shader");
        return false;
    }

    const auto compiler = vulkan_sdk_path + std::string("\\Bin\\glslangValidator.exe ");
    const auto command = compiler + "--target-env vulkan1.2 -V " + inShader.string() + " -o " + outBinary.string();

    if (system(command.c_str()) != 0) {
        std::cout << "failed to compile vulkan shader: " << inShader.string() << '\n';
        return false;
    } else {
        return true;
    }
}



void Shader::create(Device& device, const std::string& filepath) {
    this->filepath = filepath;
    
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return;

    const size_t filesize = static_cast<size_t>(file.tellg());
    spirv.resize(filesize);
    file.seekg(0);
    file.read((char*)&spirv[0], filesize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size();
    createInfo.pCode = spirv.data();

    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vk shader module");
    }

    SpvReflectShaderModule reflectModule;
    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &reflectModule);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    switch (reflectModule.spirv_execution_model) {
        case SpvExecutionModel::SpvExecutionModelVertex: {
            stage = VK_SHADER_STAGE_VERTEX_BIT;
        } break;
        case SpvExecutionModel::SpvExecutionModelFragment: {
            stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        } break;
        case SpvExecutionModel::SpvExecutionModelClosestHitKHR: {
            stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        } break;
        case SpvExecutionModel::SpvExecutionModelMissKHR: {
            stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        } break;
        case SpvExecutionModel::SpvExecutionModelRayGenerationKHR: {
            stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        } break;
    }
}



void Shader::destroy(Device& device) {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, module, nullptr);
        module = VK_NULL_HANDLE;
    }
}



VkPipelineShaderStageCreateInfo Shader::getPipelineCreateInfo() const {
    VkPipelineShaderStageCreateInfo stage_info = {};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = stage;
    stage_info.module = module;
    stage_info.pName = "main";
    return stage_info;
}

} // Raekor::VK