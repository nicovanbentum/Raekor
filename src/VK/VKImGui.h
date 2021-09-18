#pragma once

namespace Raekor::VK {

class Device;

class GUI {
public:
    void init(Device& device, SDL_Window* window);
    void destroy();

    void newFrame(SDL_Window* window);
    void createFonts(VkCommandBuffer commandBuffer);
    void record(VkCommandBuffer commandBuffer);

private:
    VkRenderPass renderPass;
};


}
