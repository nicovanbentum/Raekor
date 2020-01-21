#include "pch.h"
#include "buffer.h"
#include "VKShader.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "VKRenderer.h"
#include "VKSwapchain.h"
#include "VKDescriptor.h"

namespace Raekor {
namespace VK {

Renderer::Renderer(const VK::Context& ctx) {
    auto swapchain = VK::Swapchain(ctx, { 1920, 1080 }, VK_PRESENT_MODE_FIFO_KHR);
    VK::Shader::Compile("shaders/Vulkan/vulkan.vert", "shaders/Vulkan/vert.spv");
    auto shader = VK::Shader(ctx, "shaders/Vulkan/vert.spv");

    std::vector<Vertex> vertices;
    vertices.push_back(Vertex());
    vertices.push_back(Vertex());
    vertices.push_back(Vertex());
    vertices.push_back(Vertex());
    vertices.push_back(Vertex());
    vertices.push_back(Vertex());
    vertices.push_back(Vertex());
    std::vector<Index> indices;
    indices.push_back(Index());
    indices.push_back(Index());
    indices.push_back(Index());
    indices.push_back(Index());
    indices.push_back(Index());
    auto vertexBuffer = VK::VertexBuffer(ctx, vertices);
    auto indexBuffer = VK::IndexBuffer(ctx, indices);

    auto uniformBuffer = VK::UniformBuffer(ctx, sizeof(uint32_t));
    int x = 5;
    uniformBuffer.update(&x, sizeof(uint32_t));
    
    auto image = Stb::Image(RGBA);
    image.load("resources/textures/test.png", true);
    auto texture = VK::Texture(ctx, image);
    auto handle = texture.getImage();

    auto descSet = VK::DescriptorSet(ctx);
    descSet.add(uniformBuffer, VK_SHADER_STAGE_VERTEX_BIT);
    descSet.complete(ctx);

    auto layout = descSet.getLayout();
    auto depthTexture = DepthTexture(ctx, { 1920, 1080 });
}

} // VK
} // Raekor