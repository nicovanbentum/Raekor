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


void SetFont(const String& inPath)
{
	ImGuiIO& io = ImGui::GetIO();
	ImFont* pFont = io.Fonts->AddFontFromFileTTF(inPath.c_str(), 15.0f);
	if (!io.Fonts->Fonts.empty())
	{
		io.FontDefault = io.Fonts->Fonts.back();
	}

	// merge in icons from Font Awesome
	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("Assets/Fonts/" FONT_ICON_FILE_NAME_FAS, 14.0f, &icons_config, icons_ranges);
	// use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid

	// Build the font texture on the CPU, TexID set to NULL for the OpenGL backend
	io.Fonts->Build();
	io.Fonts->TexID = NULL;
}


void SetDarkTheme()
{
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.84f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.84f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.07f, 0.07f, 0.07f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.88f, 0.00f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.38f, 0.77f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.38f, 0.77f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.72f, 0.72f, 0.72f, 0.38f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.38f, 0.77f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.73f, 0.60f, 0.15f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.87f, 0.87f, 0.87f, 0.35f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ImGui::GetStyle().TabRounding = 2.0f;
	ImGui::GetStyle().GrabRounding = 0.0f;
	ImGui::GetStyle().PopupRounding = 0.0f;
	ImGui::GetStyle().ChildRounding = 0.0f;
	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().WindowRounding = 0.0f;
	ImGui::GetStyle().ScrollbarRounding = 0.0f;

	ImGui::GetStyle().FrameBorderSize = 1.0f;
	ImGui::GetStyle().ChildBorderSize = 0.0f;
	ImGui::GetStyle().WindowBorderSize = 0.0f;
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

	if (!hasvalue)
		frame_col = ImGui::GetColorU32(ImVec4(0.5, 0, 0, 1));
	
	RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

	ImGui::PushStyleColor(ImGuiCol_Text, hasvalue ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

	RenderTextClipped(frame_bb.Min, frame_bb.Max, text, text + strlen(text), NULL, ImVec2(0.5f, 0.5f));

	ImGui::PopStyleColor();

	if (label_size.x > 0.0f)
		RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

	return ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered && hasvalue;
}