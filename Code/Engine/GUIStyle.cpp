#include "pch.h"
#include "GUIStyle.h"

namespace RK::GUI {

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

} // namespace RK::GUI
