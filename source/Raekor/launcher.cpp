#include "pch.h"
#include "launcher.h"
#include "gui.h"

namespace Raekor {

SDL_Image::~SDL_Image() {
    if (m_Texture)   
        SDL_DestroyTexture(m_Texture);
    if (m_Surface)   
        SDL_FreeSurface(m_Surface);
    if (m_PixelData) 
        stbi_image_free(m_PixelData);
}
    


bool SDL_Image::Load(SDL_Renderer* inRenderer, const Path& inPath) {
    auto width = 0, height = 0, ch = 0;
    const auto filepath = inPath.string();
    m_PixelData = stbi_load(filepath.c_str(), &width, &height, &ch, 0);

    if (!m_PixelData)
        return false;

    const auto pitch = ((width * ch) + 3) & ~3;
    const int r = 0x000000FF, g = 0x0000FF00, b = 0x00FF0000, a = (ch == 4) ? 0xFF000000 : 0;
    m_Surface = SDL_CreateRGBSurfaceFrom(m_PixelData, width, height, ch * 8, pitch, r, g, b, a);

    if (!m_Surface) {
        stbi_image_free(m_PixelData);
        return false;
    }

    assert(inRenderer);
    m_Texture = SDL_CreateTextureFromSurface(inRenderer, m_Surface);

    if (!m_Texture)
        return false;

    return true;
}



Launcher::Launcher() : Application(WindowFlag::HIDDEN) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "";

    if (!m_Settings.mFontFile.empty())
        GUI::SetFont(m_Settings.mFontFile);
    GUI::SetTheme();

    m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_PRESENTVSYNC);
    
    SDL_RendererInfo renderer_info;
    SDL_GetRendererInfo(m_Renderer, &renderer_info);
    std::cout << "Created SDL_Renderer with name: \"" << renderer_info.name << "\"\n";

    ImGui_ImplSDL2_InitForSDLRenderer(m_Window, m_Renderer);
    ImGui_ImplSDLRenderer_Init(m_Renderer);
    SDL_SetWindowTitle(m_Window, "Launcher");

    //if (!m_BgImage.Load(m_Renderer, "assets/system/doom.jpg"))
    //    assert(false);
}



Launcher::~Launcher() {
    ImGui_ImplSDL2_Shutdown();
    ImGui_ImplSDLRenderer_Shutdown();
    SDL_DestroyRenderer(m_Renderer);
    ImGui::DestroyContext();
}



void Launcher::OnUpdate(float inDeltaTime) {
    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    auto window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    if (m_BgImage.IsLoaded())
        window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##launcher", (bool*)1, window_flags);

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

    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_Renderer);

    if (m_BgImage.IsLoaded())
        SDL_RenderCopy(m_Renderer, m_BgImage.GetTexture(), NULL, NULL);

    ImGui::Render();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(m_Renderer);

    // This part will resize the SDL window to exactly fit the ImGui content
    if ((empty_window_space.x > 0 || empty_window_space.y > 0) && m_ResizeCounter <= 2) {
        auto w = 0, h = 0;
        SDL_GetWindowSize(m_Window, &w, &h);
        SDL_SetWindowSize(m_Window, w - empty_window_space.x, h - empty_window_space.y);

        // At this point the window should be perfectly fitted to the ImGui content, the window was created with HIDDEN, so now we can SHOW it
        SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.mDisplayIndex),
            SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.mDisplayIndex));
        SDL_ShowWindow(m_Window);

        m_ResizeCounter++;
    }
}



void Launcher::OnEvent(const SDL_Event& inEvent) {
    ImGui_ImplSDL2_ProcessEvent(&inEvent);

    if (inEvent.type == SDL_WINDOWEVENT && inEvent.window.event == SDL_WINDOWEVENT_CLOSE)
            m_WasClosed = true;
}

} // raekor
