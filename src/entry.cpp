#include "pch.h"
#include "app.h"
#include "camera.h"
#include "renderer.h"

namespace Raekor {

void handle_sdl_gui_events(std::vector<SDL_Window*> windows, Raekor::Camera& camera, bool mouseInViewport, double dt) {
    auto flags = SDL_GetWindowFlags(windows[0]);
    bool focus = (flags & SDL_WINDOW_INPUT_FOCUS) ? true : false;

    if (!focus && !camera.is_mouse_active()) {
        camera.set_mouse_active(true);
        SDL_SetRelativeMouseMode(static_cast<SDL_bool>(!camera.is_mouse_active()));
    }

    if (!camera.is_mouse_active()) {
        camera.move_on_input(dt);
    }

    static bool cameraLooking = false;
    static bool cameraMoving = false;
    
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        //handle window events
        if (ev.type == SDL_WINDOWEVENT) {
            if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                for (SDL_Window* window : windows) {
                    if (SDL_GetWindowID(window) == ev.window.windowID) {
                        Application::running = false;
                    }
                }
            }
            else if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                Application::shouldResize = true;
            }
        }
        // handle mouse down events
        else if (ev.type == SDL_MOUSEBUTTONDOWN) {
            if (ev.button.button == SDL_BUTTON_RIGHT) {
                cameraLooking = true;
            }
            else if (ev.button.button == SDL_BUTTON_MIDDLE) {
                cameraMoving = true;
            }
        }

        //handle mouse up events
        else if (ev.type == SDL_MOUSEBUTTONUP) {
            if (ev.button.button == SDL_BUTTON_RIGHT) {
                cameraLooking = false;
            }
            else if (ev.button.button == SDL_BUTTON_MIDDLE) {
                cameraMoving = false;
            }
        }

        // handle mousewheel events
        else if (ev.type == SDL_MOUSEWHEEL && mouseInViewport) {
            camera.zoom(static_cast<float>(ev.wheel.y * 0.1f), dt);
        }

        // handle mouse motion events
        else if (ev.type == SDL_MOUSEMOTION) {
            if (cameraLooking) {
                camera.look(ev.motion.xrel, ev.motion.yrel, dt);
            }
            else if (cameraMoving) {
                camera.move({ ev.motion.xrel * -0.001f, ev.motion.yrel * 0.001f }, dt);
            }
            else if (!camera.is_mouse_active()) {
                camera.look(ev.motion.xrel, ev.motion.yrel, dt);
            }
        }

        // key down and not repeating a hold
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
            switch (ev.key.keysym.sym) {
            case SDLK_LALT: {
                camera.set_mouse_active(!camera.is_mouse_active());
                SDL_SetRelativeMouseMode((SDL_bool)(!camera.is_mouse_active()));
            } break;

            case SDLK_h: {
                Application::showUI = !Application::showUI;
            } break;
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&ev);
    }
}

} // namespace Raekor

int main(int argc, char** argv) {
    
    auto app = Raekor::Application();
    app.run();
    //app.serialize_settings("config.json", true);
    return 0;
}