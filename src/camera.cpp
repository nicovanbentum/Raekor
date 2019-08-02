#include "pch.h"
#include "camera.h"

namespace Raekor {

Camera::Camera(glm::vec3 position, float fov) :
    position(position),
    angle(static_cast<float>(M_PI), 0.0f), 
    FOV(fov), look_speed(0.0005f),
    mouse_active(true) {
    projection = glm::perspectiveFovRH_ZO(glm::radians(FOV), 1280.0f, 720.0f, 0.1f, 10000.0f);
}

void Camera::update(const glm::mat4& model) {
    auto dir = get_direction();
    view = glm::lookAtRH(position, position + dir, {0, 1, 0});
    mvp = projection * view * model;
}

void Camera::update() {
    auto dir = get_direction();
    view = glm::lookAt(position, position + dir, {0, 1, 0});
    view = glm::mat4(glm::mat3(view));
    mvp = projection * view * glm::mat4(1.0f);
}

void Camera::remove_translation() {
    view = glm::mat4(glm::mat3(view));
}

glm::vec3 Camera::get_direction() {
    return glm::vec3(cos(angle.y) * sin(angle.x), 
    sin(angle.y), cos(angle.y) * cos(angle.x));
}
    
void Camera::look(int x, int y) {
    angle.x += look_speed * (x * -1);
    angle.y += look_speed * (y * -1);
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

glm::mat4 Camera::get_dx_mvp(const glm::mat4& gl_m) {
    auto dir = get_direction();
    auto gl_v = glm::lookAtRH(position, position + dir, {0, 1, 0});
    auto gl_p = glm::perspectiveFovRH_ZO(glm::radians(FOV), 1280.0f, 720.0f, 0.1f, 10000.0f);
    glm::mat4 gl_mvp = gl_p * gl_v * gl_m;
    return glm::transpose(gl_mvp);

}

}
