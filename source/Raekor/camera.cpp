#include "pch.h"
#include "camera.h"
#include "input.h"

namespace Raekor {

Camera::Camera(glm::vec3 position, glm::mat4 proj) :
    m_Position(position),
    m_Angle(static_cast<float>(M_PI), 0.0f) {
    m_Projection = proj;
}


void Camera::OnUpdate(float dt) {
    auto dir = GetForwardVector();
    m_View = glm::lookAtRH(m_Position, m_Position + dir, { 0, 1, 0 });

    if (SDL_GetRelativeMouseMode()) {
        if (Input::sIsKeyPressed(SDL_SCANCODE_W)) {
            Zoom(float(zoomConstant * dt));
        }
        else if (Input::sIsKeyPressed(SDL_SCANCODE_S)) {
            Zoom(float(-zoomConstant * dt));
        }

        if (Input::sIsKeyPressed(SDL_SCANCODE_A)) {
            Move({ moveConstant * dt, 0.0f });
        }
        else if (Input::sIsKeyPressed(SDL_SCANCODE_D)) {
            Move({ -moveConstant * dt, 0.0f });
        }
    }
}


void Camera::Zoom(float amount) {
    auto dir = GetForwardVector();
    m_Position += dir * (amount * zoomSpeed);
}


void Camera::Look(glm::vec2 amount) {
    m_Angle.x += float(amount.x * -1);
    m_Angle.y += float(amount.y * -1);
    // clamp to roughly half pi so we dont overshoot
    m_Angle.y = std::clamp(m_Angle.y, -1.57078f, 1.57078f);
}


void Camera::Move(glm::vec2 amount) {
    auto dir = GetForwardVector();
    // sideways
    m_Position += glm::normalize(glm::cross(dir, { 0.0f, 1.0f, 0.0f })) * (amount.x * -moveSpeed);
    // up and down
    m_Position.y += float(amount.y * moveSpeed);
}


glm::vec3 Camera::GetForwardVector() {
    return glm::vec3(cos(m_Angle.y) * sin(m_Angle.x),
        sin(m_Angle.y), cos(m_Angle.y) * cos(m_Angle.x));
}


float Camera::GetFov() const {
    return 2.0f * atan(1.0f / m_Projection[1][1]) * 180.0f / (float)M_PI;
}


float Camera::GetFar() const { 
    return m_Projection[3][2] / (m_Projection[2][2] + 1.0f);
}


float Camera::GetNear() const { 
    return m_Projection[3][2] / (m_Projection[2][2] - 1.0f);
}


float Camera::GetAspectRatio() const {
    return m_Projection[1][1] / m_Projection[0][0];
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


glm::mat4 Viewport::GetJitteredProjMatrix(const glm::vec2& inJitter) const {
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
