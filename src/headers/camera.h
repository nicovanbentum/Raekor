#pragma once

#include "pch.h"

namespace Raekor {

class Camera {
    
public:
    Camera(glm::vec3 position, float fov);
    void look(int x, int y);
    void move_on_input(float amount);
    void update(const glm::mat4& model = glm::mat4(1.0f));
    inline glm::mat4& get_mvp() { return mvp; }
    inline bool is_mouse_active() { return mouse_active; }
    inline void set_mouse_active(bool state) { mouse_active = state; }
    glm::vec3 get_direction();

protected:
    glm::vec3 position;
    glm::vec2 angle;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 mvp;
    float FOV;
    float look_speed;
    bool mouse_active;

};

} // Namespace Raekor