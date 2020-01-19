#include "pch.h"
#include "buffer.h"
#include "VKShader.h"
#include "VKBuffer.h"
#include "VKRenderer.h"
#include "VKSwapchain.h"

namespace Raekor {
namespace VK {

Renderer::Renderer(const Context& ctx) {
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
}

} // VK
} // Raekor