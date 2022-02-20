#pragma once

#include "cvars.h"

namespace Raekor {

class Camera {
    
public:
    Camera(glm::vec3 position, glm::mat4 proj);
    Camera(const Camera&) : Camera(GetPosition(), projection) {}
    Camera& operator=(const Camera& rhs) { return *this; }

    void OnUpdate(float dt);

    void Zoom(float amount);
    void Look(glm::vec2 amount);
    void Move(glm::vec2 amount);
    
    glm::vec3 GetForwardVector();

    glm::vec2& GetAngle() { return angle; }
    const glm::vec3& GetPosition() const { return position; }

    glm::mat4& GetView() { return view; }
    const glm::mat4& GetView() const { return view; }

    glm::mat4& GetProjection() { return projection; }
    const glm::mat4& GetProjection() const { return projection; }

    float GetFov() const;
    float GetFar() const;
    float GetNear() const;
    float GetAspectRatio() const;

private:
    glm::vec2 jitter;
    glm::vec2 angle;
    glm::vec3 position;
    glm::mat4 view;
    glm::mat4 projection;

public:
    float& sensitivity = ConVars::create("sensitivity", 2.0f);
    float zoomSpeed = 1.0f, moveSpeed = 1.0f;
    float lookConstant = 1.0f, zoomConstant = 10.0f, moveConstant = 10.0f;
};


class Viewport {
public:
    Viewport(glm::vec2 size = glm::vec2(0, 0));

    void OnUpdate(float dt);
    
    float& GetFov() { return fov; }
    Camera& GetCamera() { return camera; }
    const Camera& GetCamera() const { return camera; }
    
    glm::mat4 GetJitteredProjMatrix() const;
    const glm::vec2& GetJitter() const { return jitter; }

    void SetFov(float fov);
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