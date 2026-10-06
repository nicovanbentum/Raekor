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


void SetFont(const String& inPath, float inScale)
{
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	const float font_size = glm::round(16.0f * inScale);

	ImFont* font = io.Fonts->AddFontFromFileTTF(inPath.c_str(), font_size);

	// merge in icons from Font Awesome
	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig icons_config; 
	icons_config.MergeMode = true; 
	icons_config.PixelSnapH = true;
	icons_config.GlyphMinAdvanceX = font_size;
	io.Fonts->AddFontFromFileTTF("Assets/Fonts/" FONT_ICON_FILE_NAME_FAS, font_size * 0.875f, &icons_config, icons_ranges);

	if (font)
		io.FontDefault = font;

	// Build the font texture on the CPU, TexID set to NULL for the OpenGL backend
	io.Fonts->Build();
	io.Fonts->TexID = NULL;
}


void SetDarkTheme(float inScale)
{
	ImGuiStyle& style = ImGui::GetStyle();
	style = ImGuiStyle();

	style.WindowPadding             = ImVec2(10.0f, 8.0f);
	style.FramePadding              = ImVec2(8.0f, 4.0f);
	style.CellPadding               = ImVec2(6.0f, 3.0f);
	style.ItemSpacing               = ImVec2(8.0f, 5.0f);
	style.ItemInnerSpacing          = ImVec2(6.0f, 4.0f);
	style.IndentSpacing             = 16.0f;
	style.ScrollbarSize             = 12.0f;
	style.GrabMinSize               = 10.0f;

	style.WindowBorderSize          = 1.0f;
	style.ChildBorderSize           = 1.0f;
	style.PopupBorderSize           = 1.0f;
	style.FrameBorderSize           = 0.0f;
	style.TabBorderSize             = 0.0f;
	style.DockingSeparatorSize      = 2.0f;
	style.SeparatorTextBorderSize   = 1.0f;

	style.WindowRounding            = 4.0f;
	style.ChildRounding             = 4.0f;
	style.FrameRounding             = 3.0f;
	style.PopupRounding             = 4.0f;
	style.ScrollbarRounding         = 6.0f;
	style.GrabRounding              = 3.0f;
	style.TabRounding               = 3.0f;

	style.WindowTitleAlign          = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition  = ImGuiDir_None;
	style.SeparatorTextAlign        = ImVec2(0.0f, 0.5f);
	style.SeparatorTextPadding      = ImVec2(0.0f, 4.0f);

	const ImVec4 text           = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
	const ImVec4 text_disabled  = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
	const ImVec4 background_0   = ImVec4(0.075f, 0.075f, 0.082f, 1.00f);
	const ImVec4 background_1   = ImVec4(0.110f, 0.110f, 0.122f, 1.00f);
	const ImVec4 background_2   = ImVec4(0.153f, 0.153f, 0.169f, 1.00f);
	const ImVec4 background_3   = ImVec4(0.200f, 0.200f, 0.220f, 1.00f);
	const ImVec4 background_4   = ImVec4(0.255f, 0.255f, 0.282f, 1.00f);
	const ImVec4 border         = ImVec4(0.035f, 0.035f, 0.040f, 1.00f);
	const ImVec4 accent         = ImVec4(0.259f, 0.522f, 0.957f, 1.00f);
	const ImVec4 accent_hovered = ImVec4(0.341f, 0.596f, 0.976f, 1.00f);
	const ImVec4 accent_active  = ImVec4(0.204f, 0.439f, 0.835f, 1.00f);

	auto WithAlpha = [](ImVec4 inColor, float inAlpha) { inColor.w = inAlpha; return inColor; };

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text]                   = text;
	colors[ImGuiCol_TextDisabled]           = text_disabled;
	colors[ImGuiCol_WindowBg]               = background_1;
	colors[ImGuiCol_ChildBg]                = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_PopupBg]                = WithAlpha(background_1, 0.98f);
	colors[ImGuiCol_Border]                 = border;
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_FrameBg]                = background_2;
	colors[ImGuiCol_FrameBgHovered]         = background_3;
	colors[ImGuiCol_FrameBgActive]          = background_4;
	colors[ImGuiCol_TitleBg]                = background_0;
	colors[ImGuiCol_TitleBgActive]          = background_0;
	colors[ImGuiCol_TitleBgCollapsed]       = background_0;
	colors[ImGuiCol_MenuBarBg]              = background_0;
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_ScrollbarGrab]          = background_3;
	colors[ImGuiCol_ScrollbarGrabHovered]   = background_4;
	colors[ImGuiCol_ScrollbarGrabActive]    = accent_active;
	colors[ImGuiCol_CheckMark]              = accent_hovered;
	colors[ImGuiCol_SliderGrab]             = accent;
	colors[ImGuiCol_SliderGrabActive]       = accent_hovered;
	colors[ImGuiCol_Button]                 = background_3;
	colors[ImGuiCol_ButtonHovered]          = background_4;
	colors[ImGuiCol_ButtonActive]           = accent_active;
	colors[ImGuiCol_Header]                 = WithAlpha(accent, 0.45f);
	colors[ImGuiCol_HeaderHovered]          = WithAlpha(accent, 0.30f);
	colors[ImGuiCol_HeaderActive]           = WithAlpha(accent, 0.60f);
	colors[ImGuiCol_Separator]              = background_3;
	colors[ImGuiCol_SeparatorHovered]       = accent_hovered;
	colors[ImGuiCol_SeparatorActive]        = accent;
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_ResizeGripHovered]      = WithAlpha(accent, 0.65f);
	colors[ImGuiCol_ResizeGripActive]       = accent;
	colors[ImGuiCol_InputTextCursor]        = text;
	colors[ImGuiCol_Tab]                    = background_0;
	colors[ImGuiCol_TabHovered]             = background_3;
	colors[ImGuiCol_TabSelected]            = background_1;
	colors[ImGuiCol_TabSelectedOverline]    = accent;
	colors[ImGuiCol_TabDimmed]              = background_0;
	colors[ImGuiCol_TabDimmedSelected]      = background_1;
	colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(accent, 0.0f);
	colors[ImGuiCol_DockingPreview]         = WithAlpha(accent, 0.55f);
	colors[ImGuiCol_DockingEmptyBg]         = background_0;
	colors[ImGuiCol_PlotLines]              = text_disabled;
	colors[ImGuiCol_PlotLinesHovered]       = accent_hovered;
	colors[ImGuiCol_PlotHistogram]          = accent;
	colors[ImGuiCol_PlotHistogramHovered]   = accent_hovered;
	colors[ImGuiCol_TableHeaderBg]          = background_2;
	colors[ImGuiCol_TableBorderStrong]      = border;
	colors[ImGuiCol_TableBorderLight]       = background_2;
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
	colors[ImGuiCol_TextLink]               = accent_hovered;
	colors[ImGuiCol_TextSelectedBg]         = WithAlpha(accent, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = accent_hovered;
	colors[ImGuiCol_NavCursor]              = accent;
	colors[ImGuiCol_NavWindowingHighlight]  = WithAlpha(text, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

	style.ScaleAllSizes(inScale);
}


float GetDisplayScale(SDL_Window* inWindow)
{
	const float scale = inWindow ? SDL_GetWindowDisplayScale(inWindow) : 1.0f;
	return scale > 0.0f ? scale : 1.0f;
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