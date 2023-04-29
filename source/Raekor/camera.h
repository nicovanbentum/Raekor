#pragma once

#include "cvars.h"

namespace Raekor {

struct Frustum;

class Camera {
    
public:
    Camera(glm::vec3 inPosition, glm::mat4 inProjMatrix);
    Camera(const Camera&) : Camera(m_Position, m_Projection) {}
    Camera& operator=(const Camera& rhs) { return *this; }

    void OnUpdate(float inDeltaTime);

    void Zoom(float inAmount);
    void Look(glm::vec2 inAmount);
    void Move(glm::vec2 inAmount);
    
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
    float& mSensitivity = CVars::sCreate("sensitivity", 2.0f);
    float mZoomSpeed = 1.0f, mMoveSpeed = 1.0f;
    float mLookConstant = 1.0f, mZoomConstant = 10.0f, mMoveConstant = 10.0f;
};


class Viewport {
public:
    Viewport(glm::vec2 inSize = glm::vec2(0, 0));

    void OnUpdate(float inDeltaTime);
    
    Camera& GetCamera() { return m_Camera; }
    const Camera& GetCamera() const { return m_Camera; }
    
    float GetFieldOfView() const { return m_FieldOfView; }
    void SetFieldOfView(float inFieldOfView) { m_FieldOfView = inFieldOfView; UpdateProjectionMatrix(); }

    float GetAspectRatio() const { return m_AspectRatio; }
    void SetAspectRatio(float inAspectRatio) { m_AspectRatio = inAspectRatio; UpdateProjectionMatrix(); }

    glm::vec2 GetJitter() const { return m_Jitter; }
    void SetJitter(const glm::vec2& inJitter) { m_Jitter = inJitter; }

    inline const glm::uvec2& GetSize() const { return size; }
    void SetSize(const glm::uvec2& inSize) { size = inSize; UpdateProjectionMatrix(); m_JitterIndex = 0; }

    glm::mat4 GetJitteredProjMatrix(const glm::vec2& inJitter) const;
    glm::mat4 GetJitteredProjMatrix() const { return GetJitteredProjMatrix(GetJitter()); }

public:
    // These two are public out of convenience
    glm::uvec2 size   = glm::uvec2(0u);
    glm::uvec2 offset = glm::uvec2(0u);

private:
    void UpdateProjectionMatrix();

    float m_FieldOfView = 45.0f;
    float m_AspectRatio = 16.0f / 9.0f;
    
    uint32_t m_JitterIndex = 0;
    glm::vec2 m_Jitter = glm::vec2(0.0f);

    Camera m_Camera;
};

} // Namespace Raekor