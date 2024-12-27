#pragma once

#include "Engine/Scene.h"

namespace RK {
class Widgets;
class Viewport;
}

struct ImVec2;
struct ImVec4;
typedef unsigned int ImU32;

namespace RK::GUI {

void BeginFrame();
void EndFrame();

void SetFont(const String& inFilePath);
void SetDarkTheme();

IVec2 GetMousePosWindow(const Viewport& viewport, ImVec2 windowPos);


} // namespace GUI


namespace ImGui {

// Full credit goes to https://github.com/ocornut/imgui/issues/1901
bool Spinner(const char* label, float radius, int thickness, const ImU32& color);

bool DragVec3(const char* label, glm::vec3& v, float step, float min, float max, const char* format = "%.2f");

void SetNextItemRightAlign(const char* label);

bool DragDropTargetButton(const char* label, const char* text, bool hasvalue);

}