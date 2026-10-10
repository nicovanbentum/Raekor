#include "PCH.h"
#include "GUI.h"
#include "Engine/Camera.h"
#include "IconsFontAwesome5.h"

namespace RK::GUI {

void BeginFrame()
{
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
}


void EndFrame()
{
	ImGui::EndFrame();
	ImGui::Render();
}


IVec2 GetMousePosWindow(const Viewport& inViewport, ImVec2 inMousePos)
{
	// get mouse position in window
	Vec2 mouse_pos;
	SDL_GetMouseState(&mouse_pos.x, &mouse_pos.y);

	// get mouse position relative to viewport
	IVec2 window_mouse_pos = { ( mouse_pos.x - inMousePos.x ), ( mouse_pos.y - inMousePos.y ) };

	// flip mouse coords for opengl
	window_mouse_pos.y = std::max(inViewport.GetRenderSize().y - window_mouse_pos.y, 0u);
	window_mouse_pos.x = std::max(window_mouse_pos.x, 0);

	return window_mouse_pos;
}

} // namespace Raekor


// Full credit goes to https://github.com/ocornut/imgui/issues/1901
bool ImGui::Spinner(const char* label, float radius, int thickness, const ImU32& color)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);

	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size(( radius ) * 2, ( radius + style.FramePadding.y ) * 2);

	const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
	ItemSize(bb, style.FramePadding.y);
	if (!ItemAdd(bb, id))
		return false;

	// Render
	window->DrawList->PathClear();

	int num_segments = 30;
	int start = abs(ImSin(g.Time * 1.8f) * ( num_segments - 5 ));

	const float a_min = IM_PI * 2.0f * ( (float)start ) / (float)num_segments;
	const float a_max = IM_PI * 2.0f * ( (float)num_segments - 3 ) / (float)num_segments;

	const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

	for (int i = 0; i < num_segments; i++)
	{
		const float a = a_min + ( (float)i / (float)num_segments ) * ( a_max - a_min );
		window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + g.Time * 8) * radius,
			centre.y + ImSin(a + g.Time * 8) * radius));
	}

	window->DrawList->PathStroke(color, false, thickness);

	return true;
}


bool ImGui::DragVec3(const char* label, glm::vec3& v, float step, float min, float max, const char* format)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	bool value_changed = false;
	ImGui::BeginGroup();
	ImGui::PushID(label);
	ImGui::PushMultiItemsWidths(v.length(), ImGui::CalcItemWidth());

	static const std::array colors =
	{
		ImVec4 { 0.5f, 0.0f, 0.0f, 1.0f },
		ImVec4 { 0.0f, 0.5f, 0.0f, 1.0f },
		ImVec4 { 0.1f, 0.1f, 1.0f, 1.0f }
	};

	for (int i = 0; i < v.length(); i++)
	{
		ImGui::PushID(i);
		if (i > 0)
			ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);

		const ImGuiDataType type = ImGuiDataType_Float;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, colors[i]);
		value_changed |= ImGui::DragScalar("", type, (void*)&v[i], step, &min, &max, format, 0);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		ImGui::PopID();
		ImGui::PopItemWidth();
	}

	ImGui::PopID();

	ImGui::EndGroup();
	return value_changed;
}


void ImGui::SetNextItemRightAlign(const char* label)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
}


bool ImGui::DragDropTargetButton(const char* label, const char* text, bool hasvalue)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const float w = CalcItemWidth();

	const ImVec2 label_size = CalcTextSize(label, NULL, true);
	const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
	const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

	ItemSize(total_bb, style.FramePadding.y);
	if (!ItemAdd(total_bb, id, &frame_bb, 0))
		return false;

	const bool hovered = ItemHoverable(frame_bb, id, ImGuiItemFlags_None);

	// Draw frame
	RenderNavHighlight(frame_bb, id, ImGuiNavHighlightFlags_AlwaysDraw | ImGuiNavHighlightFlags_Compact);

	ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);

	
	RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

	ImGui::PushStyleColor(ImGuiCol_Text, hasvalue ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

	RenderTextClipped(frame_bb.Min, frame_bb.Max, text, text + strlen(text), NULL, ImVec2(0.5f, 0.5f));

	ImGui::PopStyleColor();

	if (label_size.x > 0.0f)
		RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

	return ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered && hasvalue;
}