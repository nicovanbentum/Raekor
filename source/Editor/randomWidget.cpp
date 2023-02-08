#include "pch.h"
#include "randomWidget.h"
#include "editor.h"
#include "renderer.h"
#include "Raekor/systems.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(RandomWidget) {}

RandomWidget::RandomWidget(Editor* editor) :
    IWidget(editor, " Random ")
{}

void RandomWidget::draw(float dt) {
    auto& renderer = IWidget::GetRenderer();

    ImGui::Begin(m_Title.c_str(), &m_Visible);
    ImGui::SetItemDefaultFocus();

    if (ImGui::Checkbox("VSync", (bool*)(&renderer.settings.vsync))) {
        SDL_GL_SetSwapInterval(renderer.settings.vsync);
    }

    ImGui::SameLine();

    if (ImGui::Checkbox("TAA", (bool*)(&renderer.settings.enableTAA))) {
        renderer.m_FrameNr = 0;
    }

    ImGui::NewLine(); 
    ImGui::Text("VCTGI");
    ImGui::Separator();

    ImGui::DragFloat("Radius", &renderer.m_Voxelize->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine(); 
    ImGui::Text("CSM");
    ImGui::Separator();

    if (ImGui::DragFloat("Bias constant", &renderer.m_ShadowMaps->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.m_ShadowMaps->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Split lambda", &renderer.m_ShadowMaps->settings.cascadeSplitLambda, 0.0001f, 0.0f, 1.0f, "%.4f")) {
        renderer.m_ShadowMaps->updatePerspectiveConstants(m_Editor->GetViewport());
    }

    ImGui::NewLine(); 
    ImGui::Text("Bloom");
    ImGui::Separator();
    ImGui::DragFloat3("Threshold", glm::value_ptr(renderer.m_DeferredShading->settings.bloomThreshold), 0.01f, 0.0f, 10.0f, "%.3f");

    ImGui::NewLine(); 
    ImGui::Text("Theme");
    ImGui::Separator();

    static constexpr auto gImGuiColStringNames = std::array {
        "ImGuiCol_Text",
        "ImGuiCol_TextDisabled",
        "ImGuiCol_WindowBg",
        "ImGuiCol_ChildBg",
        "ImGuiCol_PopupBg",
        "ImGuiCol_Border",
        "ImGuiCol_BorderShadow",
        "ImGuiCol_FrameBg",
        "ImGuiCol_FrameBgHovered",
        "ImGuiCol_FrameBgActive",
        "ImGuiCol_TitleBg",
        "ImGuiCol_TitleBgActive",
        "ImGuiCol_TitleBgCollapsed",
        "ImGuiCol_MenuBarBg",
        "ImGuiCol_ScrollbarBg",
        "ImGuiCol_ScrollbarGrab",
        "ImGuiCol_ScrollbarGrabHovered",
        "ImGuiCol_ScrollbarGrabActive",
        "ImGuiCol_CheckMark",
        "ImGuiCol_SliderGrab",
        "ImGuiCol_SliderGrabActive",
        "ImGuiCol_Button",
        "ImGuiCol_ButtonHovered",
        "ImGuiCol_ButtonActive",
        "ImGuiCol_Header",
        "ImGuiCol_HeaderHovered",
        "ImGuiCol_HeaderActive",
        "ImGuiCol_Separator",
        "ImGuiCol_SeparatorHovered",
        "ImGuiCol_SeparatorActive",
        "ImGuiCol_ResizeGrip",
        "ImGuiCol_ResizeGripHovered",
        "ImGuiCol_ResizeGripActive",
        "ImGuiCol_Tab",
        "ImGuiCol_TabHovered",
        "ImGuiCol_TabActive",
        "ImGuiCol_TabUnfocused",
        "ImGuiCol_TabUnfocusedActive",
        "ImGuiCol_DockingPreview",
        "ImGuiCol_DockingEmptyBg",
        "ImGuiCol_PlotLines",
        "ImGuiCol_PlotLinesHovered",
        "ImGuiCol_PlotHistogram",
        "ImGuiCol_PlotHistogramHovered",
        "ImGuiCol_TableHeaderBg",
        "ImGuiCol_TableBorderStrong",
        "ImGuiCol_TableBorderLight",
        "ImGuiCol_TableRowBg",
        "ImGuiCol_TableRowBgAlt",
        "ImGuiCol_TextSelectedBg",
        "ImGuiCol_DragDropTarget",
        "ImGuiCol_NavHighlight",
        "ImGuiCol_NavWindowingHighlight",
        "ImGuiCol_NavWindowingDimBg",
        "ImGuiCol_ModalWindowDimBg"
    };

    static ImGuiCol current_theme_item = 0;
    ImGui::Combo("Active Item##ThemeColorsListBox", &current_theme_item, gImGuiColStringNames.data(), gImGuiColStringNames.size(), gImGuiColStringNames.size() / 2);
    if (ImGui::ColorEdit4("Item Color##ThemeColorsColorEdit4", m_Editor->GetSettings().themeColors[current_theme_item].data(), ImGuiColorEditFlags_Uint8))
        GUI::SetTheme(m_Editor->GetSettings().themeColors);

    // ImGui::ShowStyleEditor();

    ImGui::End();
}

} // raekor

