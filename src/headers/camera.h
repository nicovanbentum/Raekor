#pragma once

#include "pch.h"

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

class Camera {
    
public:
    Camera(glm::vec3 position, glm::mat4 proj);
    
    glm::vec3 getDirection();

    void look(float x, float y);
    void zoom(float amount = 1.0f);
    void move(glm::vec2 amount = {});
    
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

private:
    glm::vec3 position;
    glm::vec2 angle;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 mvp;
    std::array<glm::vec4, 6> frustrumPlanes;


public:
    // constants for continues movement
    // e.g camera.zoom(camera.zoomConstant);
    float lookConstant = 1.0f, zoomConstant = 0.01f, moveConstant = 0.005f;

    // speed relative to some other factor
    // e.g camera.zoom(scrollAmount * zoomSpeed);
    float lookSpeed = 0.0015f, zoomSpeed = 0.2f, moveSpeed = 0.002f;

};

class Viewport {
public:
    Viewport(SDL_Rect display) : 
        fov(45.0f), 
        aspectRatio(16.0f / 9.0f),
        camera(glm::vec3(0, 1.0, 0), glm::perspectiveRH(glm::radians(fov), aspectRatio, 0.1f, 10000.0f)), 
        size(glm::vec2(display.w, display.h))
    {

    }

    void setFov(float fov) {
        this->fov = fov;
        camera.getProjection() = glm::perspectiveRH(glm::radians(fov), 16.0f / 9.0f, 0.1f, 10000.0f);
    }

    void setAspectRatio(float ratio) {
        this->aspectRatio = ratio;
        camera.getProjection() = glm::perspectiveRH(glm::radians(fov), 16.0f / 9.0f, 0.1f, 10000.0f);
    }

    inline Camera& getCamera() {
        return camera;
    }


private:
    float fov;
    float aspectRatio;
    Camera camera;

public:
    glm::uvec2 size;

};

} // Namespace Raekor