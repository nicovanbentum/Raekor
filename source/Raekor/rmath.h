#pragma once

#include "camera.h"

namespace Raekor::Math {

struct Ray {
    Ray(Viewport& viewport, glm::vec2 coords);
    Ray(const glm::vec3& origin, const glm::vec3& direction);

    glm::vec3 origin;
    glm::vec3 direction;
    glm::vec3 rcpDirection;

    std::optional<float> HitsOBB(const glm::vec3& min, const glm::vec3& max, const glm::mat4& modelMatrix);
    std::optional<float> HitsAABB(const glm::vec3& min, const glm::vec3& max);
    std::optional<float> HitsTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
    std::optional<float> HitsSphere(const glm::vec3& o, float radius, float t_min, float t_max);
};


struct Frustrum {
    enum Halfspace {
        NEGATIVE = -1,
        ON_PLANE = 0,
        POSITIVE = 1
    };

    std::array<glm::vec4, 6> planes;

    Frustrum() = default;

    Frustrum(const glm::mat4& vp, bool normalize) {
        Create(vp, normalize);
    }

    void Create(const glm::mat4& vp, bool normalize);
    bool ContainsAABB(const glm::vec3& min, const glm::vec3& max);
};


bool gPointInAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max);

} // namespace Raekor::Math