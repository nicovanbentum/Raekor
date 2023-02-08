#pragma once

#include "cvars.h"

namespace Raekor {

struct Frustum;

class Camera {
    
public:
    Camera(glm::vec3 position, glm::mat4 proj);
    Camera(const Camera&) : Camera(m_Position, m_Projection) {}
    Camera& operator=(const Camera& rhs) { return *this; }

    void OnUpdate(float dt);

    void Zoom(float amount);
    void Look(glm::vec2 amount);
    void Move(glm::vec2 amount);
    
    glm::vec3 GetForwardVector();

    glm::vec2& GetAngle() { return m_Angle; }
    const glm::vec3& GetPosition() const { return m_Position; }

    glm::mat4& GetView() { return m_View; }
    const glm::mat4& GetView() const { return m_View; }

    glm::mat4& GetProjection() { return m_Projection; }
    const glm::mat4& GetProjection() const { return m_Projection; }

    float GetFov() const;
    float GetFar() const;
    float GetNear() const;
    float GetAspectRatio() const;

    Frustum GetFrustum() const;

private:
    glm::vec2 m_Angle;
    glm::vec3 m_Position;

    glm::mat4 m_View;
    glm::mat4 m_Projection;

public:
    float& sensitivity = CVars::sCreate("sensitivity", 2.0f);
    float zoomSpeed = 1.0f, moveSpeed = 1.0f;
    float lookConstant = 1.0f, zoomConstant = 10.0f, moveConstant = 10.0f;
};


class Viewport {
public:
    Viewport(glm::vec2 size = glm::vec2(0, 0));

    void OnUpdate(float dt);
    
    float& GetFov()                 { return fov; }
    const float& GetFov() const     { return fov; }

    Camera& GetCamera()             { return camera; }
    const Camera& GetCamera() const { return camera; }
    
    const glm::vec2& GetJitter() const { return jitter; }
    glm::mat4 GetJitteredProjMatrix(const glm::vec2& inJitter) const;
    glm::mat4 GetJitteredProjMatrix() const { return GetJitteredProjMatrix(GetJitter()); }

    void SetFov(float inFov);
    void Resize(glm::vec2 newSize);
    void SetAspectRatio(float ratio);

private:
    float fov = 45.0f;
    float aspectRatio = 16.0f / 9.0f;
    Camera camera;

public:
    uint32_t jitterIndex = 0;
    glm::vec2 jitter = glm::vec2(0.0f);

    glm::uvec2 size = glm::uvec2(0u);
    glm::uvec2 offset = glm::uvec2(0u);
};

} // Namespace Raekor