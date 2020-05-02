#include "pch.h"
#include "app.h"
#include "camera.h"
#include "renderer.h"

namespace Raekor {

void handleEvents(SDL_Window* window, Raekor::Camera& camera, bool mouseInViewport, double dt) {
    auto flags = SDL_GetWindowFlags(window);
    bool windowHasInputFocus = (flags & SDL_WINDOW_INPUT_FOCUS) ? true : false;

    // press left Alt to go enable FPS-style camera controls
    static bool inAltMode = false;
    // free the mouse if the window loses focus
    if (!windowHasInputFocus && inAltMode) {
        inAltMode = false;
        SDL_SetRelativeMouseMode(static_cast<SDL_bool>(inAltMode));
    }

    auto keyboardState = SDL_GetKeyboardState(NULL);
    if (inAltMode) {
        if (keyboardState[SDL_SCANCODE_W]) {
            camera.zoom(float(camera.zoomConstant * dt));
        } else if (keyboardState[SDL_SCANCODE_S]) {
            camera.zoom(float(-camera.zoomConstant * dt));
        }

        if (keyboardState[SDL_SCANCODE_A]) {
            camera.move({ -camera.moveConstant * dt, 0.0f });
        } else if (keyboardState[SDL_SCANCODE_D]) {
            camera.move({ camera.moveConstant * dt, 0.0f });
        }
    }

    static bool cameraLooking = false;
    static bool cameraMoving = false;
    
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        //handle window events
        if (ev.type == SDL_WINDOWEVENT) {
            if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (SDL_GetWindowID(window) == ev.window.windowID) {
                    Application::running = false;
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
            camera.zoom(static_cast<float>(ev.wheel.y * camera.zoomSpeed));
        }

        // handle mouse motion events
        else if (ev.type == SDL_MOUSEMOTION) {
            if (cameraLooking) {
                camera.look(ev.motion.xrel * camera.lookSpeed, ev.motion.yrel * camera.lookSpeed);
            }
            else if (cameraMoving) {
                camera.move({ ev.motion.xrel * -camera.moveSpeed, ev.motion.yrel * camera.moveSpeed });
            }
            else if (inAltMode) {
                camera.look(ev.motion.xrel * camera.lookSpeed, ev.motion.yrel * camera.lookSpeed);
            }
        }

        // key down and not repeating a hold
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
            switch (ev.key.keysym.sym) {
            case SDLK_LALT: {
                inAltMode = !inAltMode;
                SDL_SetRelativeMouseMode(static_cast<SDL_bool>(inAltMode));
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
    //app.serializeSettings("config.json", true);
    return 0;
}