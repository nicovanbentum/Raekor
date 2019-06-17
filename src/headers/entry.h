#pragma once

#include "pch.h"
#include "util.h"
#include "camera.h"

void handle_sdl_gui_events(SDL_Window* window, Raekor::Camera& camera) {
    auto flags = SDL_GetWindowFlags(window);
    bool focus = (flags & SDL_WINDOW_INPUT_FOCUS) ? true : false;

    if (!focus && !camera.is_mouse_active()) {
        camera.set_mouse_active(true);
        SDL_SetRelativeMouseMode(static_cast<SDL_bool>(!camera.is_mouse_active()));
    }
    
    if (!camera.is_mouse_active()) {
        camera.move_on_input(0.1f);
        camera.update();
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT)
            exit(1);

        if (!camera.is_mouse_active() && ev.type == SDL_MOUSEMOTION) {
            camera.look(ev.motion.xrel, ev.motion.yrel);
        }

        // key down and not repeating a hold
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
            switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: {
                    exit(1);
                } break;
                case SDLK_LALT: {
                    camera.set_mouse_active(!camera.is_mouse_active());                   
                    SDL_SetRelativeMouseMode((SDL_bool)(!camera.is_mouse_active()));
                } break;
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&ev);
    }
}
