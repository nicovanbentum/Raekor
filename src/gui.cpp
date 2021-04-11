#include "pch.h"
#include "gui.h"
#include "systems.h"
#include "platform/OS.h"
#include "application.h"
#include "mesh.h"
#include "assimp.h"

#include "IconsFontAwesome5.h"

namespace Raekor
{
namespace gui
{

void setTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data) {
    // load the UI's theme from config
    ImVec4* colors = ImGui::GetStyle().Colors;
    for (unsigned int i = 0; i < data.size(); i++) {
        auto& savedColor = data[i];
        ImGuiCol_Text;
        colors[i] = ImVec4(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    }

    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
    ImGui::GetStyle().WindowBorderSize = 0.0f;
    ImGui::GetStyle().ChildBorderSize = 0.0f;
    ImGui::GetStyle().FrameBorderSize = 0.0f;
}

void setFont(const std::string& filepath) {
    auto& io = ImGui::GetIO();
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(filepath.c_str(), 15.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("resources/" FONT_ICON_FILE_NAME_FAS, 15.0f, &icons_config, icons_ranges);
    // use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid
}

glm::ivec2 getMousePosWindow(const Viewport& viewport, ImVec2 windowPos) {
    // get mouse position in window
    glm::ivec2 mousePosition;
    SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

    // get mouse position relative to viewport
    glm::ivec2 rendererMousePosition = { (mousePosition.x - windowPos.x), (mousePosition.y - windowPos.y) };

    // flip mouse coords for opengl
    rendererMousePosition.y = std::max(viewport.size.y - rendererMousePosition.y, 0u);
    rendererMousePosition.x = std::max(rendererMousePosition.x, 0);

    return rendererMousePosition;
}

} // gui
} // raekor