#pragma once

#include "camera.h"

namespace Raekor {
namespace Math {

struct Ray {
    Ray() = default;
    Ray(Viewport& viewport, glm::vec2 coords);

    glm::vec3 origin;
    glm::vec3 direction;

    std::optional<float> hitsOBB(const glm::vec3& min, const glm::vec3& max, const glm::mat4& modelMatrix);
    std::optional<float> hitsAABB(const glm::vec3& min, const glm::vec3& max);
    std::optional<float> hitsTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
        auto p1 = v1 - v0;
        auto p2 = v2 - v0;
        auto pvec = glm::cross(direction, p2);
        float det = glm::dot(p1, pvec);

        if (det < std::numeric_limits<float>::epsilon()) return {};

        float invDet = 1 / det;
        auto tvec = origin - v0;
        auto u = glm::dot(tvec, pvec) * invDet;
        if (u < 0 || u > 1) return {};

        auto qvec = glm::cross(tvec, p1);
        auto v = glm::dot(direction, qvec) * invDet;
        if (v < 0 || u + v > 1) return {};

        return glm::dot(p2, qvec) * invDet;
    }
};

struct Frustrum {
    enum Halfspace {
        NEGATIVE = -1,
        ON_PLANE = 0,
        POSITIVE = 1
    };

    std::array<glm::vec4, 6> planes;

    void update(const glm::mat4& vp, bool normalize);
    bool vsAABB(const glm::vec3& min, const glm::vec3& max);
};

bool pointInAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max);

} // raekor
} // math