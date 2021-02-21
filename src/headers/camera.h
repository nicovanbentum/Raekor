#pragma once

#include "cvars.h"

namespace Raekor {

struct MVP {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 lightPos;
    glm::vec4 lightAngle;
    glm::mat4 lightSpaceMatrix;
    glm::vec3 dirLightPosition;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Camera {
    
public:
    Camera(glm::vec3 position, glm::mat4 proj);
    Camera(const Camera&) : Camera(getPosition(), projection) {}
    Camera& operator=(const Camera& rhs) { return *this; }

    glm::vec3 getDirection();

    void look(float x, float y);
    void zoom(float amount = 1.0f);
    void move(glm::vec2 amount = {});

    void onEventEditor(const SDL_Event& event);

    void strafeWASD(float dt);
    void strafeMouse(SDL_Event& event, float dt);
    
    void update(bool normalizePlanes);

    // getters
    inline glm::mat4 getMatrix(bool transpose) { return (transpose ? glm::transpose(mvp) : mvp); }
    inline glm::mat4& getView() { return view; }
    inline glm::vec2& getAngle() { return angle; }
    inline glm::mat4& getProjection() { return projection; }
    inline const glm::vec3& getPosition() { return position; }

private:
    void updatePlanes(bool normalize);
    bool vertexInPlane(const glm::vec4& plane, const glm::vec3& v);

    glm::vec3 position;
    glm::vec2 angle;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 mvp;
    std::array<glm::vec4, 6> frustrumPlanes;

public:
    float& sensitivity = ConVars::create<float>("sensitivity", 2.0f);

    // constants for continues movement
    // e.g camera.zoom(camera.zoomConstant);
    float lookConstant = 1.0f, zoomConstant = 0.01f, moveConstant = 0.005f;

    // speed relative to some other factor
    // e.g camera.zoom(scrollAmount * zoomSpeed);
    float zoomSpeed = 1.0f, moveSpeed = 0.015f;

};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Viewport {
public:
    Viewport(glm::vec2 size = glm::vec2(0, 0));
    float& getFov();
    Camera& getCamera();
    
    void setFov(float fov);
    void resize(glm::vec2 newSize);
    void setAspectRatio(float ratio);

private:
    float fov;
    float aspectRatio;
    Camera camera;

public:
    glm::uvec2 size;
    glm::uvec2 offset;
};

} // Namespace Raekor