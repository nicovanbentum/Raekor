#pragma once

#include "pch.h"
#include "util.h"

struct GE_camera {
    vec3 position = vec3(0, 0, 5);
    float angle_h = static_cast<float>(M_PI);
    float angle_v = 0.0f;
    float FOV = 45.0f;
    float speed = 0.1f;
    float look_speed = 0.0005f;
    bool mouse_active = true;
    mat4 projection;
    mat4 view;
};

void calculate_view(SDL_Window* window, GE_camera& camera) {
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    camera.angle_h += camera.look_speed * float(w / 2 - mx);
    camera.angle_v += camera.look_speed * float(h / 2 - my);

    vec3 direction(cos(camera.angle_v) * sin(camera.angle_h),
                sin(camera.angle_v),
                cos(camera.angle_v) * cos(camera.angle_h));
    vec3 right(sin(camera.angle_v - 3.14f / 2.0f), 0,
            cos(camera.angle_h - 3.14f / 2.0f));
    vec3 up(0, 1, 0);

    const uint8_t* keyboard = SDL_GetKeyboardState(NULL);
    if (keyboard[SDL_SCANCODE_W]) {
        camera.position += direction * camera.speed;
    } else if (keyboard[SDL_SCANCODE_S]) {
        camera.position -= direction * camera.speed;
    }

    camera.projection =
        glm::perspective(glm::radians(camera.FOV), 16.0f / 9.0f, 0.1f, 100.0f);
    camera.view = glm::lookAt(camera.position, camera.position + direction, up);
}

void handle_sdl_gui_events(SDL_Window* window, bool& mouse_active) {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    auto flags = SDL_GetWindowFlags(window);
    bool focus = (flags & SDL_WINDOW_INPUT_FOCUS) ? true : false;
    if (!focus && !mouse_active) {
        mouse_active = true;
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT)
            exit(1);

        // key down and not repeating a hold
        else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
            switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: {
                    exit(1);
                } break;
                case SDLK_LALT: {
                    mouse_active = !mouse_active;
                } break;
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&ev);
    }

    if (!mouse_active) {
        SDL_WarpMouseInWindow(window, w / 2, h / 2);
    }
}
