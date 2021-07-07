#pragma once

namespace Raekor::VK {

class Context;

class GUI {
public:
    void init(Context& context, SDL_Window* window);
    void destroy();

    void newFrame(SDL_Window* window);
    void createFonts(VkCommandBuffer commandBuffer);
    void record(VkCommandBuffer commandBuffer);

private:
    VkRenderPass renderPass;
};


}
