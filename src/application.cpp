#include "pch.h"
#include "application.h"

namespace Raekor {

WindowApplication::WindowApplication(RenderAPI api) : 
    window(nullptr), running(true), shouldResize(false), settings("config.json") 
{
    int sdlError = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdlError == 0, "failed to init SDL for video");

    Uint32 windowRendererFlag;
    switch (api) {
        case RenderAPI::OPENGL: {
            windowRendererFlag = SDL_WINDOW_OPENGL;
        } break;
        case RenderAPI::VULKAN: {
            windowRendererFlag = SDL_WINDOW_VULKAN;
        } break;
        default: {
            windowRendererFlag = SDL_WINDOW_OPENGL;
        }
    }

    Uint32 wflags = SDL_WINDOW_RESIZABLE | windowRendererFlag |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    settings.display = settings.display > displays.size() - 1 ? 0 : settings.display;
    window = SDL_CreateWindow(settings.name.c_str(),
        displays[settings.display].x,
        displays[settings.display].y,
        displays[settings.display].w,
        displays[settings.display].h,
        wflags);

    SDL_Rect rect;
    SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window), &rect);
    viewport = Viewport(glm::vec2(rect.w, rect.h));

    SDL_ShowWindow(window);
    SDL_MaximizeWindow(window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

WindowApplication::~WindowApplication() {
    //clean up SDL
    settings.display = SDL_GetWindowDisplayIndex(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ApplicationSettings::ApplicationSettings(const std::filesystem::path path) : path(path) {
    std::ifstream is(path);
    cereal::JSONInputArchive archive(is);
    serialize(archive);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ApplicationSettings::~ApplicationSettings() {
    std::ofstream os(path);
    cereal::JSONOutputArchive archive(os);
    serialize(archive);
}

} // namespace Raekor  