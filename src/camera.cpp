#include "pch.h"
#include "camera.h"

namespace Raekor {

Camera::Camera(glm::vec3 position, float fov) :
        position(position), FOV(fov),
        angle_h(static_cast<float>(M_PI)),
        angle_v(0.0f), look_speed(0.0005f),
        mouse_active(true)
        {}

void Camera::update() {
    auto dir = get_direction();
    glm::vec3 up(0, 1, 0);
    projection = glm::perspective(glm::radians(FOV), 16.0f / 9.0f, 0.1f, 100.0f);
    view =  glm::lookAt(position, position + dir, up);
}
    
glm::vec3 Camera::get_direction() {
    return glm::vec3(cos(angle_v) * sin(angle_h), 
    sin(angle_v), cos(angle_v) * cos(angle_h));
}
    
void Camera::add_look(int x, int y) {
    angle_h += look_speed * (x * -1);
    angle_v += look_speed * (y * -1);
}

void Camera::move_on_input(float amount) {
    auto dir = get_direction();
    const uint8_t* keyboard = SDL_GetKeyboardState(NULL);
    if (keyboard[SDL_SCANCODE_W]) {
        position += dir * amount;
        } else if (keyboard[SDL_SCANCODE_S]) {
            position -= dir * amount;
        }
}
}
