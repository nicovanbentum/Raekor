#include "pch.h"
#include "compiler.h"
#include "gui.h"
#include "archive.h"

namespace Raekor {

CompilerApp::CompilerApp() : Application(WindowFlag::NONE) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "";

    if (!m_Settings.mFontFile.empty())
        GUI::SetFont(m_Settings.mFontFile);
    
    GUI::SetTheme();
    ImGui::GetStyle().ScaleAllSizes(1.33333333f);

    m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_PRESENTVSYNC);
    
    SDL_RendererInfo renderer_info;
    SDL_GetRendererInfo(m_Renderer, &renderer_info);
    std::cout << "Created SDL_Renderer with name: \"" << renderer_info.name << "\"\n";

    ImGui_ImplSDL2_InitForSDLRenderer(m_Window, m_Renderer);
    ImGui_ImplSDLRenderer_Init(m_Renderer);
    SDL_SetWindowTitle(m_Window, "Raekor Compiler App");

    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(m_Window, &wminfo);

    // Add the system tray icon
    NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
    nid.hWnd = wminfo.info.win.window;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(nid.szTip, "Raekor Compiler App");

    Shell_NotifyIcon(NIM_ADD, &nid);

    // hide the console window
    //ShowWindow(GetConsoleWindow(), SW_HIDE);
}



CompilerApp::~CompilerApp() {
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(m_Window, &wminfo);

    NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
    nid.hWnd = wminfo.info.win.window;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(nid.szTip, "Raekor Compiler App");
    Shell_NotifyIcon(NIM_DELETE, &nid);

    ImGui_ImplSDL2_Shutdown();
    ImGui_ImplSDLRenderer_Shutdown();
    SDL_DestroyRenderer(m_Renderer);
    ImGui::DestroyContext();
}



void CompilerApp::OnUpdate(float inDeltaTime) {
    for (const auto& file : FileSystem::recursive_directory_iterator("assets")) {
        const auto path = file.path();

        if (path.extension() == ".json")
            m_Files.insert(path);
    }

    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text(reinterpret_cast<const char*>(ICON_FA_ADDRESS_BOOK));

        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("Exit"))
                m_Running = false;

            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowPos(ImGui::GetCursorPos(), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    auto window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##Compiler", (bool*)1, window_flags);

    auto flags = ImGuiTableFlags_RowBg;
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.15, 0.15, 0.15, 1.0));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.20, 0.20, 0.20, 1.0));

    if (ImGui::BeginTable("Assets", 3, flags)) {
        for (const auto& [index, file] : gEnumerate(m_Files)) {
            ImGui::TableNextColumn();
            
            auto string = file.string();
            
            ImGui::Text(string.c_str());

            ImGui::TableNextColumn();

            ImGui::Text("Hash: ");

            ImGui::TableNextColumn();

            ImGui::Text("Write time: ");

            ImGui::TableNextRow();
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    ImGui::End(); // ##Window
    ImGui::PopStyleVar(1); // ImGuiStyleVar_WindowRounding 0.0f

    ImGui::EndFrame();
    ImGui::Render();

    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_Renderer);

    ImGui::Render();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(m_Renderer);
}



void CompilerApp::OnEvent(const SDL_Event& inEvent) {
    ImGui_ImplSDL2_ProcessEvent(&inEvent);

    if (inEvent.type == SDL_WINDOWEVENT && inEvent.window.event == SDL_WINDOWEVENT_CLOSE)
            m_WasClosed = true;
}

}