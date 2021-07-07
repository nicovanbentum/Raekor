#include "pch.h"
#include "VKImGui.h"

#include "VKContext.h"

namespace Raekor::VK {


void GUI::init(Context& context, SDL_Window* window) {
    ImGui_ImplVulkan_InitInfo info = {};
    info.Device = context.device;
    info.PhysicalDevice = context.PDevice;
    info.Instance = context.instance;
    info.QueueFamily = context.device.getQueues().graphics.value();
    info.Queue = context.device.graphicsQueue;
    info.PipelineCache = VK_NULL_HANDLE;
    info.DescriptorPool = context.device.descriptorPool;
    info.MinImageCount = 2;
    info.ImageCount = info.MinImageCount;
    info.Allocator = nullptr;
    info.CheckVkResultFn = nullptr;

    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_Init(&info, renderPass);
}

void GUI::destroy() {
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}


void GUI::createFonts(VkCommandBuffer commandBuffer) {
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
}


void GUI::newFrame(SDL_Window* window) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
}


void GUI::record(VkCommandBuffer commandBuffer) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}


}