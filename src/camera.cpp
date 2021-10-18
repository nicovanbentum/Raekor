#include "pch.h"
#include "camera.h"
#include "input.h"
#include "cvars.h"

namespace Raekor {

Camera::Camera(glm::vec3 position, glm::mat4 proj) :
    position(position),
    angle(static_cast<float>(M_PI), 0.0f) {
    projection = proj;
}



void Camera::update() {
    auto dir = getForwardVector();
    view = glm::lookAtRH(position, position + dir, { 0, 1, 0 });
}



glm::vec3 Camera::getForwardVector() {
    return glm::vec3(cos(angle.y) * sin(angle.x),
        sin(angle.y), cos(angle.y) * cos(angle.x));
}



void Camera::look(float x, float y) {
    angle.x += float(x * -1);
    angle.y += float(y * -1);
    // clamp to roughly half pi so we dont overshoot
    angle.y = std::clamp(angle.y, -1.57078f, 1.57078f);
}



void Camera::zoom(float amount) {
    auto dir = getForwardVector();
    position += dir * (amount* zoomSpeed);
}



void Camera::move(glm::vec2 amount) {
    auto dir = getForwardVector();
    // sideways
    position += glm::normalize(glm::cross(dir, { 0.0f, 1.0f, 0.0f })) * amount.x;
    // up and down
    position.y += (float)(amount.y);
}



void Camera::strafeWASD(float dt) {
    if (Input::isKeyPressed(SDL_SCANCODE_W)) {
        zoom(float(zoomConstant * dt));
    } else if (Input::isKeyPressed(SDL_SCANCODE_S)) {
        zoom(float(-zoomConstant * dt));
    }

    if (Input::isKeyPressed(SDL_SCANCODE_A)) {
        move({ -moveConstant * dt, 0.0f });
    } else if (Input::isKeyPressed(SDL_SCANCODE_D)) {
        move({ moveConstant * dt, 0.0f });
    }
}



void Camera::strafeMouse(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        auto formula = glm::radians(0.022f * sensitivity);
        look((event.motion.xrel * formula), event.motion.yrel * formula);
    }
}



bool Camera::onEventEditor(const SDL_Event& event) {
    bool moved = false;

    int x, y;
    auto mouseState = SDL_GetMouseState(&x, &y);

    if (event.type == SDL_MOUSEMOTION) {
        if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            auto formula = glm::radians(0.022f * sensitivity * 2.0f);
            look(event.motion.xrel * formula, event.motion.yrel * formula);
            moved = true;
        } else if (mouseState & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
            move({ event.motion.xrel * -moveSpeed, event.motion.yrel * moveSpeed });
            moved = true;
        }
    } else if (event.type == SDL_MOUSEWHEEL) {
        zoom(static_cast<float>(event.wheel.y * zoomSpeed));
        moved = true;
    }

    return moved;
}



float Camera::getFOV() const {
    return 2.0f * atan(1.0f / projection[1][1]) * 180.0f / (float)M_PI;
}



float Camera::getAspectRatio() const {
    return projection[1][1] / projection[0][0];
}



float Camera::getNear() const { 
    return projection[3][2] / (projection[2][2] - 1.0f); 
}



float Camera::getFar() const { 
    return projection[3][2] / (projection[2][2] + 1.0f); 
}



Viewport::Viewport(glm::vec2 size) : 
    fov(65.0f), 
    aspectRatio(16.0f / 9.0f),
    camera(glm::vec3(0, 1.0, 0), glm::perspectiveRH(glm::radians(fov), aspectRatio, 0.1f, 1000.0f)),
    size(size) 
{}



float& Viewport::getFov() { return fov; }



void Viewport::setFov(float fov) {
    this->fov = fov;
    camera.getProjection() = glm::perspectiveRH(
        glm::radians(fov), 
        camera.getAspectRatio(), 
        camera.getNear(), 
        camera.getFar()
    );
}



void Viewport::setAspectRatio(float ratio) {
    this->aspectRatio = ratio;
    camera.getProjection() = glm::perspectiveRH(
        glm::radians(fov), 
        camera.getAspectRatio(), 
        camera.getNear(), 
        camera.getFar()
    );
}



Camera& Viewport::getCamera() { return camera; }



const Camera& Viewport::getCamera() const { return camera; }



void Viewport::resize(glm::vec2 newSize) {
    size = { newSize.x, newSize.y };
    camera.getProjection() = glm::perspectiveRH(
        glm::radians(camera.getFOV()), 
        float(size.x) / float(size.y), 
        camera.getNear(), 
        camera.getFar()
    );
}

}
