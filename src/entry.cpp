#include "pch.h"
#include "app.h"
#include "camera.h"

namespace Raekor {

void handle_sdl_gui_events(std::vector<SDL_Window*> windows, Raekor::Camera& camera, double dt) {
    auto flags = SDL_GetWindowFlags(windows[0]);
    bool focus = (flags & SDL_WINDOW_INPUT_FOCUS) ? true : false;

    if (!focus && !camera.is_mouse_active()) {
        camera.set_mouse_active(true);
        SDL_SetRelativeMouseMode(static_cast<SDL_bool>(!camera.is_mouse_active()));
    }

    if (!camera.is_mouse_active()) {
        camera.move_on_input(dt);
        camera.update();
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) {
            for (SDL_Window* window : windows) {
                if (SDL_GetWindowID(window) == ev.window.windowID) {
                    exit(EXIT_SUCCESS);
                }
            }
        }

        if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
            std::puts("window resized");
        }

        if (!camera.is_mouse_active() && ev.type == SDL_MOUSEMOTION) {
            camera.look(ev.motion.xrel, ev.motion.yrel);
        }

        // key down and not repeating a hold
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
            switch (ev.key.keysym.sym) {
            case SDLK_LALT: {
                camera.set_mouse_active(!camera.is_mouse_active());
                SDL_SetRelativeMouseMode((SDL_bool)(!camera.is_mouse_active()));
            } break;
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&ev);
    }
}

} // namespace Raekor

int main(int argc, char** argv) {
    
    auto app = Raekor::Application();
    app.vulkan_main();
    system("PAUSE");
    return 0;
}