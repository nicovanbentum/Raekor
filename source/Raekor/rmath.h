#pragma once

#include "camera.h"

namespace Raekor {

using Mat4x4 = glm::mat4x4;
using Mat4x3 = glm::mat4x3;
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using UVec3 = glm::uvec3;
using IVec3 = glm::ivec3;
using IVec4 = glm::ivec4;


struct BBox3D {
    BBox3D() = default;
    BBox3D(const Vec3& inMin, const Vec3& inMax) : mMin(inMin), mMax(inMax) {}

    const Vec3& GetMin() const { return mMin; }
    const Vec3& GetMax() const { return mMax; }

    bool IsValid() const { return glm::all(glm::greaterThan(mMax, mMin)); }

    BBox3D& Transform(const Mat4x4& inTransform);

    Vec3 mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 mMax = Vec3(FLT_MIN, FLT_MIN, FLT_MIN);
};


struct Ray {
    Ray(Viewport& viewport, Vec2 coords);
    Ray(const Vec3& origin, const Vec3& direction);

    Vec3 m_Origin;
    Vec3 m_Direction;
    Vec3 m_RcpDirection;

    std::optional<float> HitsOBB(const BBox3D& inOBB, const Mat4x4& modelMatrix) const;
    std::optional<float> HitsAABB(const BBox3D& inABB) const;
    std::optional<float> HitsTriangle(const Vec3& inV0, const Vec3& inV1, const Vec3& inV2) const;
    std::optional<float> HitsSphere(const Vec3& inPosition, float inRadius, float inTmin, float inTmax) const;
};


struct Frustum {
    enum Halfspace {
        NEGATIVE = -1,
        ON_PLANE = 0,
        POSITIVE = 1
    };

    std::array<Vec4, 6> m_Planes;

    Frustum() = default;
    Frustum(const glm::mat4& inViewProjMatrix, bool inShouldNormalize);

    bool ContainsAABB(const BBox3D& inAABB)const ;
};


bool gPointInAABB(const Vec3& inPoint, const BBox3D& inAABB);

} // namespace Raekor