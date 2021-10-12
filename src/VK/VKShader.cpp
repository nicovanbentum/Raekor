#include "pch.h"
#include "VKShader.h"
#include "VKDevice.h"
#include "VKUtil.h"

namespace Raekor::VK {

bool Shader::glslangValidator(const char* vulkanSDK, const fs::directory_entry& file) {
    if (!file.is_regular_file()) return false;

    const auto outfile = file.path().parent_path() / "bin" / file.path().filename();
    const auto compiler = vulkanSDK + std::string("\\Bin\\glslangValidator.exe ");
    const auto command = compiler + "--target-env vulkan1.2 -V " + file.path().string() + " -o " + std::string(outfile.string() + ".spv");

    if (system(command.c_str()) != 0) {
        return false;
    } 
    
    return true;
}



Shader Device::createShader(const std::string& filepath) {
    
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return Shader(); // TODO: ptr type

    std::vector<uint32_t> spirv;
    const size_t filesize = size_t(file.tellg());
    spirv.resize(filesize);
    file.seekg(0);
    file.read((char*)&spirv[0], filesize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size();
    createInfo.pCode = spirv.data();

    Shader shader;
    shader.filepath = filepath;
    ThrowIfFailed(vkCreateShaderModule(device, &createInfo, nullptr, &shader.module));

    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &shader.reflectModule);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    switch (shader.reflectModule.spirv_execution_model) {
        case SpvExecutionModel::SpvExecutionModelVertex: {
            shader.stage = VK_SHADER_STAGE_VERTEX_BIT;
        } break;
        case SpvExecutionModel::SpvExecutionModelFragment: {
            shader.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        } break;
        case SpvExecutionModel::SpvExecutionModelClosestHitKHR: {
            shader.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        } break;
        case SpvExecutionModel::SpvExecutionModelMissKHR: {
            shader.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        } break;
        case SpvExecutionModel::SpvExecutionModelRayGenerationKHR: {
            shader.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        } break;
    }

    return shader;
}



void Device::destroyShader(Shader& shader) {
    if (shader.module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shader.module, nullptr);
        shader.module = VK_NULL_HANDLE;
    }

    spvReflectDestroyShaderModule(&shader.reflectModule);
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