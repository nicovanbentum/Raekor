#pragma once

#include "scene.h"
#include "application.h"

namespace GUI {

void BeginDockSpace();
void EndDockSpace();

void BeginFrame();
void EndFrame();
    
void SetFont(const std::string& filepath);
void SetTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data);

glm::ivec2 GetMousePosWindow(const Raekor::Viewport& viewport, ImVec2 windowPos);

} // namespace GUI