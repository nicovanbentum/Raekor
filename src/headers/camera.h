#pragma once

#include "pch.h"
#include "scene.h"

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
    void look(int x, int y);
    void update(bool normalizePlanes);
    glm::vec3 get_direction();
    void move_on_input(double dt);
    inline bool is_mouse_active() { return mouse_active; }
    inline void set_mouse_active(bool state) { mouse_active = state; }

    // getters
    inline glm::mat4 get_mvp(bool transpose) { return (transpose ? glm::transpose(mvp) : mvp); }
    inline glm::mat4& getView() { return view; }
    inline glm::vec2& getAngle() { return angle; }
    inline glm::mat4& getProjection() { return projection; }
    inline float* get_move_speed() { return &move_speed; }
    inline float* get_look_speed() { return &look_speed; }
    inline const glm::vec3& getPosition() { return position; }

    bool isVisible(const SceneObject& object);

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
    bool mouse_active;


public:
    float look_speed;
    float move_speed;

};

} // Namespace Raekor