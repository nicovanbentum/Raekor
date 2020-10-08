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
    std::optional<float> hitsTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
    std::optional<float> hitsSphere(const glm::vec3& o, float radius, float t_min, float t_max);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////

bool pointInAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max);

} // raekor
} // math