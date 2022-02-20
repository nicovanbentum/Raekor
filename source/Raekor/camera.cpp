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


void Camera::OnUpdate(float dt) {
    auto dir = GetForwardVector();
    view = glm::lookAtRH(position, position + dir, { 0, 1, 0 });

    if (SDL_GetRelativeMouseMode()) {
        if (Input::isKeyPressed(SDL_SCANCODE_W)) {
            Zoom(float(zoomConstant * dt));
        }
        else if (Input::isKeyPressed(SDL_SCANCODE_S)) {
            Zoom(float(-zoomConstant * dt));
        }

        if (Input::isKeyPressed(SDL_SCANCODE_A)) {
            Move({ moveConstant * dt, 0.0f });
        }
        else if (Input::isKeyPressed(SDL_SCANCODE_D)) {
            Move({ -moveConstant * dt, 0.0f });
        }
    }
}


void Camera::Zoom(float amount) {
    auto dir = GetForwardVector();
    position += dir * (amount * zoomSpeed);
}


void Camera::Look(glm::vec2 amount) {
    angle.x += float(amount.x * -1);
    angle.y += float(amount.y * -1);
    // clamp to roughly half pi so we dont overshoot
    angle.y = std::clamp(angle.y, -1.57078f, 1.57078f);
}


void Camera::Move(glm::vec2 amount) {
    auto dir = GetForwardVector();
    // sideways
    position += glm::normalize(glm::cross(dir, { 0.0f, 1.0f, 0.0f })) * (amount.x * -moveSpeed);
    // up and down
    position.y += float(amount.y * moveSpeed);
}


glm::vec3 Camera::GetForwardVector() {
    return glm::vec3(cos(angle.y) * sin(angle.x),
        sin(angle.y), cos(angle.y) * cos(angle.x));
}


float Camera::GetFov() const {
    return 2.0f * atan(1.0f / projection[1][1]) * 180.0f / (float)M_PI;
}


float Camera::GetFar() const { 
    return projection[3][2] / (projection[2][2] + 1.0f); 
}


float Camera::GetNear() const { 
    return projection[3][2] / (projection[2][2] - 1.0f); 
}


float Camera::GetAspectRatio() const {
    return projection[1][1] / projection[0][0];
}


Viewport::Viewport(glm::vec2 size) : 
    camera(glm::vec3(0, 1.0, 0), glm::perspectiveRH(glm::radians(fov), aspectRatio, 0.1f, 1000.0f)),
    size(size) 
{}


void Viewport::SetFov(float fov) {
    this->fov = fov;

    camera.GetProjection() = glm::perspectiveRH(
        glm::radians(fov), 
        camera.GetAspectRatio(), 
        camera.GetNear(), 
        camera.GetFar()
    );
}


void Viewport::SetAspectRatio(float ratio) {
    this->aspectRatio = ratio;

    camera.GetProjection() = glm::perspectiveRH(
        glm::radians(fov), 
        camera.GetAspectRatio(), 
        camera.GetNear(), 
        camera.GetFar()
    );
}


float haltonSequence(uint32_t i, uint32_t b) {
    float f = 1.0f;
    float r = 0.0f;

    while (i > 0) {
        f /= float(b);
        r = r + f * float(i % b);
        i = uint32_t(floorf(float(i) / float(b)));
    }

    return r;
}


void Viewport::OnUpdate(float dt) {
    camera.OnUpdate(dt);

    jitterIndex = jitterIndex + 1;

    const auto halton = glm::vec2(
        2.0f * haltonSequence(jitterIndex + 1, 2) - 1.0f,
        2.0f * haltonSequence(jitterIndex + 1, 3) - 1.0f
    );

    jitter = halton / glm::vec2(size);
}


glm::mat4 Viewport::GetJitteredProjMatrix() const {
    auto mat = camera.GetProjection();
    mat[2][0] = jitter[0];
    mat[2][1] = jitter[1];
    return mat;
}


void Viewport::Resize(glm::vec2 newSize) {
    size = { newSize.x, newSize.y };
    
    camera.GetProjection() = glm::perspectiveRH(
        glm::radians(camera.GetFov()), 
        float(size.x) / float(size.y), 
        camera.GetNear(), 
        camera.GetFar()
    );

    jitterIndex = 0;
}

}
