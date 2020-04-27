#include "pch.h"
#include "camera.h"

namespace Raekor {

Camera::Camera(glm::vec3 position, glm::mat4 proj) :
    position(position),
    angle(static_cast<float>(M_PI), 0.0f), 
    lookSpeed(0.0015f), moveSpeed(0.01f) {
    projection = proj;
    //glm::perspectiveRH_ZO(glm::radians(FOV), 16.0f/9.0f, 0.1f, 10000.0f);

}

void Camera::update(bool normalizePlanes) {
    auto dir = getDirection();
    view = glm::lookAtRH(position, position + dir, {0, 1, 0});
    updatePlanes(normalizePlanes);
}

glm::vec3 Camera::getDirection() {
    return glm::vec3(cos(angle.y) * sin(angle.x),
    sin(angle.y), cos(angle.y) * cos(angle.x));
}
    
void Camera::look(int x, int y) {
    angle.x += float(lookSpeed * (x * -1));
    angle.y += float(lookSpeed * (y * -1));
    // clamp to roughly half pi so we dont overshoot
    angle.y = std::clamp(angle.y, -1.57078f, 1.57078f);
}

void Camera::zoom(float amount) {
    auto dir = getDirection();
    position += dir * (float)(amount);
}

void Camera::move(glm::vec2 amount) {
    auto dir = getDirection();
    // sideways
    position += glm::normalize(glm::cross(dir, { 0,1,0 })) * (float)(amount.x * 2.0);

    // up and down
    position.y += (float)(amount.y);
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

}
