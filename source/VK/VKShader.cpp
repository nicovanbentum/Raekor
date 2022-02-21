#include "pch.h"
#include "VKShader.h"
#include "VKDevice.h"
#include "VKUtil.h"

namespace Raekor::VK {

bool Shader::sCompileGLSL(const char* vulkanSDK, const fs::directory_entry& file) {
    const auto outfile = file.path().parent_path() / "bin" / file.path().filename();

    std::stringstream command;
    command // Call the compiler
            << vulkanSDK << "\\Bin\\glslangValidator.exe --target-env vulkan1.2 -V " 
            // glsl input text file
            << file.path().string()
            // compile to spirv binary file
            << " -o " << std::string(outfile.string() + ".spv");

    if (system(command.str().c_str()) != 0) {
        return false;
    } 
    
    return true;
}



bool Shader::sCompileHLSL(const fs::directory_entry& file) {
    const auto outfile = file.path().parent_path() / "bin" / file.path().filename();

    const auto name = file.path().stem().string();
    auto type = name.substr(name.size() - 2, 2);
    std::transform(type.begin(), type.end(), type.begin(), tolower);

    std::stringstream command;
    command // Call the compiler
            << "dxc -spirv -E main -fvk-stage-io-order=decl -T " 
            // stage type (e.g. ps_6_7, deduced from the last 2 characters of the file's stem
            << type << "_6_7 " 
            // hlsl input text file
            << file.path().string() 
            // compile to spirv binary file
            << " -Fo " << std::string(outfile.string() + ".spv"); 
    
    if (system(command.str().c_str()) != 0) {
        return false;
    }

    return true;
}



Shader Device::CreateShader(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Could not open shader " << filepath << '\n';
        return Shader(); // TODO: ptr type
    }

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
    gThrowIfFailed(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shader.module));

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



void Device::DestroyShader(Shader& shader) {
    if (shader.module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_Device, shader.module, nullptr);
        shader.module = VK_NULL_HANDLE;
    }

    spvReflectDestroyShaderModule(&shader.reflectModule);
}



VkPipelineShaderStageCreateInfo Shader::GetPipelineCreateInfo() const {
    VkPipelineShaderStageCreateInfo stage_info = {};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = stage;
    stage_info.module = module;
    stage_info.pName = "main";
    return stage_info;
}

} // Raekor::VK