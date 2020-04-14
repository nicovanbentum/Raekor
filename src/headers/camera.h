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

    void look(int x, int y, double dt);
    void zoom(float amount, double dt);
    void move(glm::vec2 amount, double dt);
    
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
    float lookSpeed;
    float moveSpeed;

};

} // Namespace Raekor