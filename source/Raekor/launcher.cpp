#include "pch.h"
#include "launcher.h"
#include "gui.h"

namespace Raekor {

Launcher::Launcher() : Application(WindowFlag::HIDDEN) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "";

    GUI::SetFont(m_Settings.font);
    GUI::SetTheme(m_Settings.themeColors);

    m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_SOFTWARE);
    ImGui_ImplSDL2_InitForSDLRenderer(m_Window, m_Renderer);
    ImGui_ImplSDLRenderer_Init(m_Renderer);
    SDL_SetWindowTitle(m_Window, "Raekor Launcher");
}



Launcher::~Launcher() {
    ImGui_ImplSDL2_Shutdown();
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui::DestroyContext();
}



void Launcher::OnUpdate(float inDeltaTime) {
    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    auto region_max = ImGui::GetContentRegionMaxAbs();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##window", (bool*)1, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

    ImGui::BeginTable("Configuration Settings", 2);

    for (auto& cvar : CVars::sGet()) {
        switch (cvar.second.index()) {
            case 0: { // int
                auto value = bool(std::get<0>(cvar.second));
                auto string = "##" + cvar.first;
                if (ImGui::Checkbox(string.c_str(), &value)) {
                    cvar.second = value;
                }
                ImGui::SameLine();
                ImGui::Text(cvar.first.c_str());
                ImGui::TableNextColumn();
            }
        }

        bool value = false;
    }

    ImGui::EndTable();
    ImGui::NewLine();
    ImGui::NewLine();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
    if (ImGui::Button("Launch", ImVec2(-10, 20))) {
        m_Running = false;
    }

    auto empty_window_space = ImGui::GetContentRegionAvail();

    ImGui::End(); // ##Window
    ImGui::PopStyleVar(1); // ImGuiStyleVar_WindowRounding 0.0f

    ImGui::EndFrame();
    ImGui::Render();

    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_Renderer);

    ImGui::Render();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(m_Renderer);

    // This part will resize the SDL window to perfectly fit the ImGui content
    if ((empty_window_space.x > 0 || empty_window_space.y > 0) && m_ResizeCounter <= 2) {
        int w, h;
        SDL_GetWindowSize(m_Window, &w, &h);
        SDL_SetWindowSize(m_Window, w - empty_window_space.x, h - empty_window_space.y);

        // At this point the window should be perfectly fitted to the ImGui content, the window was created with HIDDEN, so now we can SHOW it
        SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.display),
            SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.display));
        SDL_ShowWindow(m_Window);

        m_ResizeCounter++;
    }
}



void Launcher::OnEvent(const SDL_Event& inEvent) {
    ImGui_ImplSDL2_ProcessEvent(&inEvent);

    if (inEvent.type == SDL_WINDOWEVENT) {
        if (inEvent.window.event == SDL_WINDOWEVENT_CLOSE) {
            m_Running = false;
            m_WasClosed = true;
        }
    }
}

} // raekor
