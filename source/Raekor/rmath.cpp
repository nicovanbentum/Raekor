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



Ray::Ray(const glm::vec3& origin, const glm::vec3& direction) : 
    origin(origin), 
    direction(direction), 
    rcpDirection(1.0f / direction) 
{}



std::optional<float> Ray::hitsOBB(const glm::vec3& min, const glm::vec3& max, const glm::mat4& transform) {
    float tMin = 0.0f;
    float tMax = 100000.0f;

    glm::vec3 OBBposition_worldspace(transform[3]);
    glm::vec3 delta = OBBposition_worldspace - origin;

    {
        glm::vec3 xaxis(transform[0]);
        float e = glm::dot(xaxis, delta);
        float f = glm::dot(direction, xaxis);

        if (fabs(f) > 0.001f) {
            float t1 = (e + min.x) / f;
            float t2 = (e + max.x) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < tMax) tMax = t2;
            if (t1 > tMin) tMin = t1;
            if (tMin > tMax) return std::nullopt;

        }
        else {
            //if (-e + min.x > 0.0f || -e + max.x < 0.0f) return std::nullopt;
        }
    }


    {
        glm::vec3 yaxis(transform[1]);
        float e = glm::dot(yaxis, delta);
        float f = glm::dot(direction, yaxis);

        if (fabs(f) > 0.001f) {

            float t1 = (e + min.y) / f;
            float t2 = (e + max.y) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < tMax) tMax = t2;
            if (t1 > tMin) tMin = t1;
            if (tMin > tMax) return std::nullopt;

        }
        else {
            //if (-e + min.y > 0.0f || -e + max.y < 0.0f) return std::nullopt;
        }
    }


    {
        glm::vec3 zaxis(transform[2]);
        float e = glm::dot(zaxis, delta);
        float f = glm::dot(direction, zaxis);

        if (fabs(f) > 0.001f) {

            float t1 = (e + min.z) / f;
            float t2 = (e + max.z) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < tMax) tMax = t2;
            if (t1 > tMin) tMin = t1;
            if (tMin > tMax) return std::nullopt;

        }
        else {
            //if (-e + min.z > 0.0f || -e + max.z < 0.0f) return std::nullopt;
        }
    }

    return tMin;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<float> Ray::hitsAABB(const glm::vec3& min, const glm::vec3& max) {
    auto t1 = (min.x - origin.x) * rcpDirection.x;
    auto t2 = (max.x - origin.x) * rcpDirection.x;

    auto tmin = glm::min(t1, t2);
    auto tmax = glm::max(t1, t2);

    for (int i = 1; i < 3; i++) {
        t1 = (min[i] - origin[i]) * rcpDirection[i];
        t1 = (max[i] - origin[i]) * rcpDirection[i];

        tmin = glm::max(tmin, glm::min(glm::min(t1, t2), tmax));
        tmax = glm::min(tmax, glm::max(glm::max(t1, t2), tmin));
    }

    return tmax > glm::max(tmin, 0.0f) ? std::make_optional(tmax) : std::nullopt;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<float> Ray::hitsTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
    auto p1 = v1 - v0;
    auto p2 = v2 - v0;
    auto pvec = glm::cross(direction, p2);
    float det = glm::dot(p1, pvec);

    if (fabs(det) < std::numeric_limits<float>::epsilon()) return std::nullopt;

    float invDet = 1 / det;
    auto tvec = origin - v0;
    auto u = glm::dot(tvec, pvec) * invDet;
    if (u < 0 || u > 1) return std::nullopt;

    auto qvec = glm::cross(tvec, p1);
    auto v = glm::dot(direction, qvec) * invDet;
    if (v < 0 || u + v > 1) return std::nullopt;

    return glm::dot(p2, qvec) * invDet;
}



std::optional<float> Ray::hitsSphere(const glm::vec3& o, float radius, float t_min, float t_max) {
    float R2 = radius * radius;
    glm::vec3 L = o - origin;
    float tca = glm::dot(L, glm::normalize(direction));
    if (tca < 0) return std::nullopt;

    float D2 = dot(L, L) - tca * tca;
    if (D2 > R2) return std::nullopt;
    float thc = sqrt(R2 - D2);
    float t0 = tca - thc;
    float t1 = tca + thc;

    float closest_t = std::min(t0, t1);

    if (closest_t < t_max && closest_t > t_min) {
        return closest_t / glm::length(direction);
    }

    return std::nullopt;
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
            const float mag = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
            plane.x = plane.x / mag;
            plane.y = plane.y / mag;
            plane.z = plane.z / mag;
            plane.w = plane.w / mag;
        }
    }
}



bool Frustrum::vsAABB(const glm::vec3& min, const glm::vec3& max) {
    for (const auto& plane : planes) {
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