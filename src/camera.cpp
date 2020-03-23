#include "pch.h"
#include "camera.h"

namespace Raekor {

Camera::Camera(glm::vec3 position, glm::mat4 proj) :
    position(position),
    angle(static_cast<float>(M_PI), 0.0f), 
    look_speed(0.15f), move_speed(0.01f),
    mouse_active(true) {
    projection = proj;
    //glm::perspectiveRH_ZO(glm::radians(FOV), 16.0f/9.0f, 0.1f, 10000.0f);

}

void Camera::update(bool normalizePlanes) {
    auto dir = get_direction();
    view = glm::lookAtRH(position, position + dir, {0, 1, 0});
    updatePlanes(normalizePlanes);
}

glm::vec3 Camera::get_direction() {
    return glm::vec3(cos(angle.y) * sin(angle.x),
    sin(angle.y), cos(angle.y) * cos(angle.x));
}
    
void Camera::look(int x, int y, double dt) {
    angle.x += float(look_speed * (x * -1) * (dt / 1000));
    angle.y += float(look_speed * (y * -1) * (dt / 1000));
    // clamp to roughly half pi so we dont overshoot
    angle.y = std::clamp(angle.y, -1.57078f, 1.57078f);
}

void Camera::zoom(float amount, double dt) {
    auto dir = get_direction();
    position += dir * (float)(amount*dt);
}

void Camera::move(glm::vec2 amount, double dt) {
    auto dir = get_direction();
    // sideways
    position += glm::normalize(glm::cross(dir, { 0,1,0 })) * (float)(amount.x * dt);

    // up and down
    position.y += (float)(amount.y * dt);
}

void Camera::move_on_input(double dt) {
    auto dir = get_direction();
    const uint8_t* keyboard = SDL_GetKeyboardState(NULL);
    if (keyboard[SDL_SCANCODE_W]) {
        position += dir * (float)(move_speed*dt);
    } 
    else if (keyboard[SDL_SCANCODE_S]) {
            position -= dir * (float)(move_speed*dt);
    }
    if (keyboard[SDL_SCANCODE_A]) {
        position -= glm::normalize(glm::cross(dir, { 0,1,0 })) * (float)(move_speed*dt);
    }
    else if (keyboard[SDL_SCANCODE_D]) {
        position += glm::normalize(glm::cross(dir, { 0,1,0 })) * (float)(move_speed*dt);
    }
}

/*
    https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
*/
void Camera::updatePlanes(bool normalize) {
    glm::mat4 comboMatrix = projection * view;
    frustrumPlanes[0].x = comboMatrix[0][3] + comboMatrix[0][0];
    frustrumPlanes[0].y = comboMatrix[1][3] + comboMatrix[1][0];
    frustrumPlanes[0].z = comboMatrix[2][3] + comboMatrix[2][0];
    frustrumPlanes[0].w = comboMatrix[3][3] + comboMatrix[3][0];

    frustrumPlanes[1].x = comboMatrix[0][3] + comboMatrix[0][0];
    frustrumPlanes[1].y = comboMatrix[1][3] + comboMatrix[1][0];
    frustrumPlanes[1].z = comboMatrix[2][3] + comboMatrix[2][0];
    frustrumPlanes[1].w = comboMatrix[3][3] + comboMatrix[3][0];

    frustrumPlanes[2].x = comboMatrix[0][3] + comboMatrix[0][1];
    frustrumPlanes[2].y = comboMatrix[1][3] + comboMatrix[1][1];
    frustrumPlanes[2].z = comboMatrix[2][3] + comboMatrix[2][1];
    frustrumPlanes[2].w = comboMatrix[3][3] + comboMatrix[3][1];

    frustrumPlanes[3].x = comboMatrix[0][3] + comboMatrix[0][1];
    frustrumPlanes[3].y = comboMatrix[1][3] + comboMatrix[1][1];
    frustrumPlanes[3].z = comboMatrix[2][3] + comboMatrix[2][1];
    frustrumPlanes[3].w = comboMatrix[3][3] + comboMatrix[3][1];

    frustrumPlanes[4].x = comboMatrix[0][3] + comboMatrix[0][2];
    frustrumPlanes[4].y = comboMatrix[1][3] + comboMatrix[1][2];
    frustrumPlanes[4].z = comboMatrix[2][3] + comboMatrix[2][2];
    frustrumPlanes[4].w = comboMatrix[3][3] + comboMatrix[3][2];

    frustrumPlanes[5].x = comboMatrix[0][3] + comboMatrix[0][2];
    frustrumPlanes[5].y = comboMatrix[1][3] + comboMatrix[1][2];
    frustrumPlanes[5].z = comboMatrix[2][3] + comboMatrix[2][2];
    frustrumPlanes[5].w = comboMatrix[3][3] + comboMatrix[3][2];

    if (normalize) {
        for (auto& plane : frustrumPlanes) {
            float mag;
            mag = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
            plane.x = plane.x / mag;
            plane.y = plane.y / mag;
            plane.z = plane.z / mag;
            plane.w = plane.w / mag;
        }
    }
}

bool Camera::vertexInPlane(const glm::vec4& plane, const glm::vec3& v) {
    float d;
    d = plane.x*v.x + plane.y*v.y + plane.z*v.z + plane.w;
    if (d >= 0) return true;
    return false;
}

bool Camera::isVisible(const SceneObject& object) {
    int visiblePoints = 6;
    for (auto& point : object.boundingBox) {
        for (auto& plane : frustrumPlanes) {
            if (!vertexInPlane(plane, point)) {
                visiblePoints -= 1;
                break;
            }
        }
    }

    if (visiblePoints > 0) return true;
    return false;
}

}
