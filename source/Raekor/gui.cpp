#include "pch.h"
#include "gui.h"
#include "ImGuizmo.h"
#include "IconsFontAwesome5.h"

namespace GUI {

void beginDockSpace() {
    ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* imGuiViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(imGuiViewport->Pos);
    ImGui::SetNextWindowSize(imGuiViewport->Size);
    ImGui::SetNextWindowViewport(imGuiViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    dockWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
        dockWindowFlags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", (bool*)true, dockWindowFlags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
}



void endDockSpace() {
    ImGui::End();
}



void beginFrame() {
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}



void endFrame() {
    ImGui::EndFrame();
    ImGui::Render();
}



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
    
    // Build the font texture on the CPU, TexID set to NULL for the OpenGL backend
    io.Fonts->Build();
    io.Fonts->TexID = NULL;
}



glm::ivec2 getMousePosWindow(const Raekor::Viewport& viewport, ImVec2 windowPos) {
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

} // raekor