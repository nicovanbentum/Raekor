#pragma once

#include "pch.h"

namespace Raekor {

struct MVP {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 lightPos;
    glm::vec3 lightAngle;
};

class Camera {
    
public:
    Camera(glm::vec3 position, float fov);
    void look(int x, int y);
    void update();
    void set_aspect_ratio(float new_ratio);
    glm::vec3 get_direction();
    void move_on_input(double dt);
    inline bool is_mouse_active() { return mouse_active; }
    inline void set_mouse_active(bool state) { mouse_active = state; }

    // getters
    inline glm::mat4 get_mvp(bool transpose) { return (transpose ? glm::transpose(mvp) : mvp); }
    inline const glm::mat4 getView() { return view; }
    inline const glm::mat4 getProjection() { return projection; }
    inline float* get_move_speed() { return &move_speed; }
    inline float* get_look_speed() { return &look_speed; }
    inline const glm::vec3& getPosition() { return position; }

private:
    glm::vec3 position;
    glm::vec2 angle;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 mvp;
    float FOV;
    float look_speed;
    float move_speed;
    bool mouse_active;

};

} // Namespace Raekor