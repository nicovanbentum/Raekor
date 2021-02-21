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

//////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::update(bool normalizePlanes) {
    auto dir = getDirection();
    view = glm::lookAtRH(position, position + dir, { 0, 1, 0 });
    updatePlanes(normalizePlanes);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

glm::vec3 Camera::getDirection() {
    return glm::vec3(cos(angle.y) * sin(angle.x),
        sin(angle.y), cos(angle.y) * cos(angle.x));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::look(float x, float y) {
    angle.x += float(x * -1);
    angle.y += float(y * -1);
    // clamp to roughly half pi so we dont overshoot
    angle.y = std::clamp(angle.y, -1.57078f, 1.57078f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::zoom(float amount) {
    auto dir = getDirection();
    position += dir * (float)(amount);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::move(glm::vec2 amount) {
    auto dir = getDirection();

    // sideways
    position += glm::normalize(glm::cross(dir, { 0, 1, 0 })) * amount.x;

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

void Camera::strafeMouse(SDL_Event& event, float dt) {
    if (event.type == SDL_MOUSEMOTION) {
        auto formula = glm::radians(0.022f * sensitivity);
        look((event.motion.xrel * formula), event.motion.yrel * formula);
    }
}

void Camera::onEventEditor(const SDL_Event& event) {
    int x, y;
    auto mouseState = SDL_GetMouseState(&x, &y);

    if (event.type == SDL_MOUSEMOTION) {
        if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            auto formula = glm::radians(0.022f * sensitivity * 2.0f);
            look(event.motion.xrel * formula, event.motion.yrel * formula);
        } else if (mouseState & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
            move({ event.motion.xrel * -moveSpeed, event.motion.yrel * moveSpeed });
        }
    } else if (event.type == SDL_MOUSEWHEEL) {
        zoom(static_cast<float>(event.wheel.y * zoomSpeed));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

/*
    https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
*/
void Camera::updatePlanes(bool normalize) {
    glm::mat4 comboMatrix = projection * view;
    frustrumPlanes[0].x = comboMatrix[0][3] + comboMatrix[0][0];
    frustrumPlanes[0].y = comboMatrix[1][3] + comboMatrix[1][0];
    frustrumPlanes[0].z = comboMatrix[2][3] + comboMatrix[2][0];
    frustrumPlanes[0].w = comboMatrix[3][3] + comboMatrix[3][0];

    frustrumPlanes[1].x = comboMatrix[0][3] + comboMatrix[0][0];
    frustrumPlanes[1].y = comboMatrix[1][3] + comboMatrix[1][0];
    frustrumPlanes[1].z = comboMatrix[2][3] + comboMatrix[2][0];
    frustrumPlanes[1].w = comboMatrix[3][3] + comboMatrix[3][0];

    frustrumPlanes[2].x = comboMatrix[0][3] + comboMatrix[0][1];
    frustrumPlanes[2].y = comboMatrix[1][3] + comboMatrix[1][1];
    frustrumPlanes[2].z = comboMatrix[2][3] + comboMatrix[2][1];
    frustrumPlanes[2].w = comboMatrix[3][3] + comboMatrix[3][1];

    frustrumPlanes[3].x = comboMatrix[0][3] + comboMatrix[0][1];
    frustrumPlanes[3].y = comboMatrix[1][3] + comboMatrix[1][1];
    frustrumPlanes[3].z = comboMatrix[2][3] + comboMatrix[2][1];
    frustrumPlanes[3].w = comboMatrix[3][3] + comboMatrix[3][1];

    frustrumPlanes[4].x = comboMatrix[0][3] + comboMatrix[0][2];
    frustrumPlanes[4].y = comboMatrix[1][3] + comboMatrix[1][2];
    frustrumPlanes[4].z = comboMatrix[2][3] + comboMatrix[2][2];
    frustrumPlanes[4].w = comboMatrix[3][3] + comboMatrix[3][2];

    frustrumPlanes[5].x = comboMatrix[0][3] + comboMatrix[0][2];
    frustrumPlanes[5].y = comboMatrix[1][3] + comboMatrix[1][2];
    frustrumPlanes[5].z = comboMatrix[2][3] + comboMatrix[2][2];
    frustrumPlanes[5].w = comboMatrix[3][3] + comboMatrix[3][2];

    if (normalize) {
        for (auto& plane : frustrumPlanes) {
            float mag;
            mag = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
            plane.x = plane.x / mag;
            plane.y = plane.y / mag;
            plane.z = plane.z / mag;
            plane.w = plane.w / mag;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool Camera::vertexInPlane(const glm::vec4& plane, const glm::vec3& v) {
    float d;
    d = plane.x * v.x + plane.y * v.y + plane.z * v.z + plane.w;
    if (d >= 0) {
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Viewport::Viewport(glm::vec2 size) : fov(65.0f), aspectRatio(16.0f / 9.0f),
camera(glm::vec3(0, 1.0, 0), glm::perspectiveRH(glm::radians(fov), aspectRatio, 0.1f, 10000.0f)),
size(size) {}

//////////////////////////////////////////////////////////////////////////////////////////////////

float& Viewport::getFov() { return fov; }

//////////////////////////////////////////////////////////////////////////////////////////////////

void Viewport::setFov(float fov) {
    this->fov = fov;
    camera.getProjection() = glm::perspectiveRH(glm::radians(fov), 16.0f / 9.0f, 0.1f, 10000.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Viewport::setAspectRatio(float ratio) {
    this->aspectRatio = ratio;
    camera.getProjection() = glm::perspectiveRH(glm::radians(fov), 16.0f / 9.0f, 0.1f, 10000.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Camera& Viewport::getCamera() { return camera; }

//////////////////////////////////////////////////////////////////////////////////////////////////

void Viewport::resize(glm::vec2 newSize) {
    size = { newSize.x, newSize.y };
    camera.getProjection() = glm::perspectiveRH(glm::radians(fov), (float)size.x / (float)size.y, 0.1f, 10000.0f);
}

}
