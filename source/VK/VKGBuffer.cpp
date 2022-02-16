#include "pch.h"
#include "VKGBuffer.h"
#include "VKDevice.h"
#include "VKUtil.h"
#include "VKStructs.h"

namespace Raekor::VK {

GBuffer::GBuffer(Device& device, const Viewport& viewport) {
    vertexShader = device.createShader("shaders\\Vulkan\\gbuffer.vert");
    pixelShader = device.createShader("shaders\\Vulkan\\gbuffer.frag");

    uniformBuffer = device.createBuffer(
        sizeof(uniforms),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    createRenderTargets(device, viewport);
    createPipeline(device);
}



void GBuffer::createPipeline(Device& device) {
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.size = 128; // TODO: query size from physical device

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    ThrowIfFailed(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout));

    auto vertexInput = GraphicsPipeline::VertexInput()
        .binding(0, sizeof(Vertex))
        .attribute(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos))
        .attribute(1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv))
        .attribute(2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal))
        .attribute(3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent));

    auto state = GraphicsPipeline::State();
    state.colorBlend.attachmentCount = 3;

    auto pipeDesc = GraphicsPipeline::Desc();
    pipeDesc.state = &state;
    pipeDesc.vertexShader = &vertexShader;
    pipeDesc.vertexInput = &vertexInput;
    pipeDesc.pixelShader = &pixelShader;

    pipeline = device.createGraphicsPipeline(pipeDesc, framebuffer, layout);
}



void GBuffer::createRenderTargets(Device& device, const Viewport& viewport) {
    Texture::Desc texDesc;
    texDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    texDesc.width = viewport.size.x;
    texDesc.height = viewport.size.y;

    albedoTexture = device.createTexture(texDesc);
    normalTexture = device.createTexture(texDesc);
    materialTexture = device.createTexture(texDesc);

    texDesc.format = VK_FORMAT_D32_SFLOAT;
    depthTexture = device.createTexture(texDesc);

    auto fbDesc = FrameBuffer::Desc()
        .colorAttachment(0, albedoTexture)
        .colorAttachment(1, normalTexture)
        .colorAttachment(2, materialTexture)
        .depthAttachment(depthTexture);

    framebuffer = device.createFrameBuffer(fbDesc);
}



void GBuffer::destroyRenderTargets(Device& device) {
    device.destroyFrameBuffer(framebuffer);
    device.destroyTexture(albedoTexture);
    device.destroyTexture(normalTexture);
    device.destroyTexture(materialTexture);
    device.destroyTexture(depthTexture);
}


void GBuffer::record(Device& device, VkCommandBuffer commandBuffer, const Scene& scene) {
    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent.width = framebuffer.description.width;
    beginInfo.renderArea.extent.height = framebuffer.description.height;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };

    beginInfo.clearValueCount = (uint32_t)clearValues.size();
    beginInfo.pClearValues = clearValues.data();


    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    const auto view = scene.view<const RTGeometry, const Transform>();

    for (const auto& [entity, mesh, transform] : view.each()) {
        VkDeviceSize offset[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertices.buffer, offset);
    }
}

}