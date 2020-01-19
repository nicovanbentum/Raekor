#include "pch.h"
#include "app.h"
#include "util.h"
#include "timer.h"
#include "entry.h"
#include "camera.h"
#include "VK/VKContext.h"
#include "VK/VKRenderer.h"
#include "PlatformContext.h"

namespace Raekor {

void Application::doVulkan() {
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    int sdl_err = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdl_err == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto window = SDL_CreateWindow(name.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(index),
        SDL_WINDOWPOS_CENTERED_DISPLAY(index),
        static_cast<int>(displays[index].w * 0.9),
        static_cast<int>(displays[index].h * 0.9),
        wflags);

    //initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    Camera camera = Camera({ 0.0f, 1.0f, 0.0f }, 45.0f);

    try {
        ////////// NEW CODE GOES HERE /////////////
        VK::Context ctx = VK::Context(window);
        VK::Renderer renderer = VK::Renderer(ctx);

        std::puts("job well done.");

    }
    catch (std::exception e) {
        std::cout << e.what() << '\n';
    }

    SDL_ShowWindow(window);
    SDL_SetWindowInputFocus(window);

    Ffilter ft_mesh = {};
    ft_mesh.name = "Supported Mesh Files";
    ft_mesh.extensions = "*.obj;*.fbx";

    Timer dt_timer = Timer();
    double dt = 0;

    //main application loop
    while (running) {
        dt_timer.start();
        //handle sdl and imgui events
        handle_sdl_gui_events({ window }, camera, dt);

    }

    SDL_DestroyWindow(window);
    SDL_Quit();
}

} // raekor