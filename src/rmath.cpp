#include "pch.h"
#include "rmath.h"
#include "camera.h"

namespace Raekor {
namespace Math {

Ray::Ray(Viewport& viewport, glm::vec2 coords) {
    glm::vec3 rayNDC = {
        (2.0f * coords.x) / viewport.size.x - 1.0f,
        1.0f - (2.0f * coords.y) / viewport.size.y,
        1.0f
    };

    glm::vec4 rayClip = { rayNDC.x, rayNDC.y, -1.0f, 1.0f };

    glm::vec4 rayCamera = glm::inverse(viewport.getCamera().getProjection()) * rayClip;
    rayCamera.z = -1.0f, rayCamera.w = 0.0f;

    glm::vec3 rayWorld = glm::inverse(viewport.getCamera().getView()) * rayCamera;
    rayWorld = glm::normalize(rayWorld);

    direction = rayWorld;
    origin = viewport.getCamera().getPosition();
}

std::optional<float> Ray::hitsOBB(const glm::vec3& min, const glm::vec3& max, const glm::mat4& modelMatrix) {
    float tMin = 0.0f;
    float tMax = 100000.0f;

    glm::vec3 OBBposition_worldspace(modelMatrix[3].x, modelMatrix[3].y, modelMatrix[3].z);
    glm::vec3 delta = OBBposition_worldspace - origin;

    {
        glm::vec3 xaxis(modelMatrix[0].x, modelMatrix[0].y, modelMatrix[0].z);
        float e = glm::dot(xaxis, delta);
        float f = glm::dot(direction, xaxis);

        if (fabs(f) > 0.001f) {
            float t1 = (e + min.x) / f;
            float t2 = (e + max.x) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < tMax) tMax = t2;
            if (t1 > tMin) tMin = t1;
            if (tMin > tMax) return {};

        }
        else {
            if (-e + min.x > 0.0f || -e + max.x < 0.0f) return {};
        }
    }


    {
        glm::vec3 yaxis(modelMatrix[1].x, modelMatrix[1].y, modelMatrix[1].z);
        float e = glm::dot(yaxis, delta);
        float f = glm::dot(direction, yaxis);

        if (fabs(f) > 0.001f) {

            float t1 = (e + min.y) / f;
            float t2 = (e + max.y) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < tMax) tMax = t2;
            if (t1 > tMin) tMin = t1;
            if (tMin > tMax) return {};

        }
        else {
            if (-e + min.y > 0.0f || -e + max.y < 0.0f) return {};
        }
    }


    {
        glm::vec3 zaxis(modelMatrix[2].x, modelMatrix[2].y, modelMatrix[2].z);
        float e = glm::dot(zaxis, delta);
        float f = glm::dot(direction, zaxis);

        if (fabs(f) > 0.001f) {

            float t1 = (e + min.z) / f;
            float t2 = (e + max.z) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < tMax) tMax = t2;
            if (t1 > tMin) tMin = t1;
            if (tMin > tMax) return {};

        }
        else {
            if (-e + min.z > 0.0f || -e + max.z < 0.0f) return {};
        }
    }

    return tMin;
}

std::optional<float> Ray::hitsAABB(const glm::vec3& min, const glm::vec3& max) {
    auto tnear = (min.x - origin.x) / direction.x;
    auto tfar = (max.x - origin.x) / direction.x;

    if (tnear > tfar) std::swap(tnear, tfar);

    auto t1y = (min.y - origin.y) / direction.y;
    auto t2y = (max.y - origin.y) / direction.y;

    if (t1y > t2y) std::swap(t1y, t2y);

    if ((tnear > t2y) || (t1y > tfar)) return false;

    if (t1y > tnear) tnear = t1y;
    if (t2y < tfar) tfar = t2y;

    auto t1z = (min.z - origin.z) / direction.z;
    auto t2z = (max.z - origin.z) / direction.z;

    if (t1z > t2z) std::swap(t1z, t2z);

    if ((tnear > t2z) || (t1z > tfar))  return false;

    if (t1z > tnear) tnear = t1z;
    if (t2z < tfar) tfar = t2z;

    std::cout << tfar - tnear << std::endl;

    return true;
}

bool pointInAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max) {
    return  (point.x >= min.x && point.x <= max.x) &&
        (point.y >= min.y && point.y <= max.y) &&
        (point.z >= min.z && point.z <= max.z);
}

/*
    Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix by G. Gribb & K. Hartmann 
    https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
*/
void Frustrum::update(const glm::mat4& vp, bool normalize) {
    planes[0].x = vp[0][3] + vp[0][0];
    planes[0].y = vp[1][3] + vp[1][0];
    planes[0].z = vp[2][3] + vp[2][0];
    planes[0].w = vp[3][3] + vp[3][0];

    planes[1].x = vp[0][3] - vp[0][0];
    planes[1].y = vp[1][3] - vp[1][0];
    planes[1].z = vp[2][3] - vp[2][0];
    planes[1].w = vp[3][3] - vp[3][0];

    planes[2].x = vp[0][3] - vp[0][1];
    planes[2].y = vp[1][3] - vp[1][1];
    planes[2].z = vp[2][3] - vp[2][1];
    planes[2].w = vp[3][3] - vp[3][1];

    planes[3].x = vp[0][3] + vp[0][1];
    planes[3].y = vp[1][3] + vp[1][1];
    planes[3].z = vp[2][3] + vp[2][1];
    planes[3].w = vp[3][3] + vp[3][1];

    planes[4].x = vp[0][3] + vp[0][2];
    planes[4].y = vp[1][3] + vp[1][2];
    planes[4].z = vp[2][3] + vp[2][2];
    planes[4].w = vp[3][3] + vp[3][2];

    planes[5].x = vp[0][3] - vp[0][2];
    planes[5].y = vp[1][3] - vp[1][2];
    planes[5].z = vp[2][3] - vp[2][2];
    planes[5].w = vp[3][3] - vp[3][2];

    if (normalize) {
        for (auto& plane : planes) {
            float mag;
            mag = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
            plane.x = plane.x / mag;
            plane.y = plane.y / mag;
            plane.z = plane.z / mag;
            plane.w = plane.w / mag;
        }
    }
}

bool Frustrum::vsAABB(const glm::vec3& min, const glm::vec3& max) {
    for (auto& plane : planes) {
        int out = 0;
        out += ((glm::dot(plane, glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
        if (out == 8) return false;
    }

    return true;
}

} // raekor
} // math