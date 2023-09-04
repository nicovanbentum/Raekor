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


namespace ImGui {

// Full credit goes to https://github.com/ocornut/imgui/issues/1901
bool Spinner(const char* label, float radius, int thickness, const ImU32& color);

}