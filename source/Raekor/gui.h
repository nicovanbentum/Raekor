#pragma once

#include "scene.h"
#include "application.h"

namespace GUI {

void beginDockSpace();
void endDockSpace();

void beginFrame();
void endFrame();
    
void setTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data);

void setFont(const std::string& filepath);

glm::ivec2 getMousePosWindow(const Raekor::Viewport& viewport, ImVec2 windowPos);

} // GUI