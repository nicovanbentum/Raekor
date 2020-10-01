#include "pch.h"
#include "input.h"

namespace Raekor {

bool InputHandler::handleEvents(WindowApplication* app, bool mouseInViewport, double dt) {
    auto flags = SDL_GetWindowFlags(app->window);
    bool windowHasInputFocus = (flags & SDL_WINDOW_INPUT_FOCUS) ? true : false;
    auto& camera = app->viewport.getCamera();

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
        }
        else if (keyboardState[SDL_SCANCODE_S]) {
            camera.zoom(float(-camera.zoomConstant * dt));
        }

        if (keyboardState[SDL_SCANCODE_A]) {
            camera.move({ -camera.moveConstant * dt, 0.0f });
        }
        else if (keyboardState[SDL_SCANCODE_D]) {
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
                if (SDL_GetWindowID(app->window) == ev.window.windowID) {
                    app->running = false;
                }
            }
            else if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                app->shouldResize = true;
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
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&ev);
    }

    return inAltMode;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Input> Input::singleton = std::make_unique<Input>();

//////////////////////////////////////////////////////////////////////////////////////////////////

Input::Input() :
    keyboardState(SDL_GetKeyboardState(NULL))
{}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool Input::isKeyPressed(SDL_Keycode code) {
    return singleton->keyboardState[code];
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool Input::isButtonPressed(unsigned int button) {
    return singleton->mouseState & SDL_BUTTON(button);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Input::setCallback(Uint32 type, const std::function<void()>& callback) {

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Input::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

    }
}

} // raekor