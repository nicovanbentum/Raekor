#pragma once

#include "scene.h"
#include "application.h"
#include "renderer.h"

namespace Raekor {
namespace gui {

void setTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data);

void setFont(const std::string& filepath);

glm::ivec2 getMousePosWindow(const Viewport& viewport, ImVec2 windowPos);

} // gui
} // raekor