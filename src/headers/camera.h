#pragma once

#include "cvars.h"

namespace Raekor {

class Camera {
    
public:
    Camera(glm::vec3 position, glm::mat4 proj);
    Camera(const Camera&) : Camera(getPosition(), projection) {}
    Camera& operator=(const Camera& rhs) { return *this; }

    glm::vec3 getForwardVector();

    void look(float x, float y);
    void zoom(float amount);
    void move(glm::vec2 amount);

    bool onEventEditor(const SDL_Event& event);

    void strafeWASD(float dt);
    void strafeMouse(const SDL_Event& event);
    
    void update();

    inline glm::vec2& getAngle() { return angle; }
    inline const glm::vec3& getPosition() const { return position; }

    inline glm::mat4& getView() { return view; }
    inline const glm::mat4& getView() const { return view; }

    inline glm::mat4& getProjection() { return projection; }
    inline const glm::mat4& getProjection() const { return projection; }

    float getFOV() const;
    float getAspectRatio() const;
    float getNear() const;
    float getFar() const;

private:
    uint32_t jitterIndex = 0;
    glm::vec2 jitter;
    glm::vec2 angle;
    glm::vec3 position;
    glm::mat4 view;
    glm::mat4 projection;

public:
    float& sensitivity = ConVars::create("sensitivity", 2.0f);
    float lookConstant = 1.0f, zoomConstant = 0.01f, moveConstant = 0.005f;
    float zoomSpeed = 1.0f, moveSpeed = 0.015f;

};



class Viewport {
public:
    Viewport(glm::vec2 size = glm::vec2(0, 0));
    float& getFov();

    Camera& getCamera();
    const Camera& getCamera() const;

    void update();
    glm::mat4 getJitteredProjMatrix() const;
    const glm::vec2& getJitter() const { return jitter; }
    
    void setFov(float fov);
    void resize(glm::vec2 newSize);
    void setAspectRatio(float ratio);

private:
    float fov;
    float aspectRatio;
    Camera camera;

public:
    uint32_t jitterIndex = 0;
    glm::vec2 jitter = glm::vec2(0.0f);
    glm::uvec2 size = glm::uvec2(0u);
    glm::uvec2 offset = glm::uvec2(0u);
};

} // Namespace Raekor