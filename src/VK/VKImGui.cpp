#include "pch.h"
#include "VKImGui.h"

#include "VKUtil.h"
#include "VKDevice.h"

namespace Raekor::VK {


void GUI::init(Device& device, SDL_Window* window) {
    ImGui_ImplVulkan_InitInfo info = {};
    info.Device = device;
    //info.PhysicalDevice = physicalDevice;
    //info.Instance = instance; // TODO
    info.QueueFamily = device.getQueueFamilyIndex();
    info.Queue = device.getQueue();
    info.PipelineCache = VK_NULL_HANDLE;
    info.DescriptorPool = device.descriptorPool;
    info.MinImageCount = 2;
    info.ImageCount = info.MinImageCount;
    info.Allocator = nullptr;
    info.CheckVkResultFn = ThrowIfFailed;

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