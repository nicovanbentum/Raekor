#pragma once

#include "scene.h"
#include "application.h"

namespace Raekor::GUI {

void BeginDockSpace();
void EndDockSpace();

void BeginFrame();
void EndFrame();
    
void SetFont(const std::string& inFilePath);
void SetTheme();

glm::ivec2 GetMousePosWindow(const Viewport& viewport, ImVec2 windowPos);

} // namespace GUI