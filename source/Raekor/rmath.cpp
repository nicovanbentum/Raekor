#include "pch.h"
#include "rmath.h"
#include "camera.h"

namespace Raekor {

static std::default_random_engine sDefaultRandomEngine;
static std::uniform_real_distribution<float> sUniformDistribution01(0.0, 1.0);


BBox3D& BBox3D::Transform(const Mat4x4& inTransform) {
    mMin = inTransform * Vec4(mMin, 1.0);
    mMax = inTransform * Vec4(mMax, 1.0);
    return *this;
}



Ray::Ray(Viewport& inViewport, Vec2 inCoords) {
    Vec3 ray_ndc = {
        (2.0f * inCoords.x) / inViewport.size.x - 1.0f,
        1.0f - (2.0f * inCoords.y) / inViewport.size.y,
        1.0f
    };

    const auto clip_ray = Vec4 { ray_ndc.x, ray_ndc.y, -1.0f, 1.0f };

    auto camera_ray = glm::inverse(inViewport.GetCamera().GetProjection()) * clip_ray;
    camera_ray.z = -1.0f, camera_ray.w = 0.0f;

    auto world_ray = glm::inverse(inViewport.GetCamera().GetView()) * camera_ray;
    world_ray = glm::normalize(world_ray);

    m_Direction = world_ray;
    m_Origin = inViewport.GetCamera().GetPosition();
}


Ray::Ray(const glm::vec3& inOrigin, const glm::vec3& inDirection) : 
    m_Origin(inOrigin),
    m_Direction(inDirection),
    m_RcpDirection(1.0f / inDirection)
{}


std::optional<float> Ray::HitsOBB(const BBox3D& inOBB, const Mat4x4& inTransform) const {
    float t_min = 0.0f;
    float t_max = 100000.0f;

    const auto obb_pos_ws = glm::vec3(inTransform[3]);
    const auto delta = obb_pos_ws - m_Origin;

    {
        const auto xaxis = glm::vec3(inTransform[0]);
        const auto e = glm::dot(xaxis, delta);
        const auto f = glm::dot(m_Direction, xaxis);

        if (fabs(f) > 0.001f) {
            auto t1 = (e + inOBB.mMin.x) / f;
            auto t2 = (e + inOBB.mMax.x) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < t_max) t_max = t2;
            if (t1 > t_min) t_min = t1;
            if (t_min > t_max) return std::nullopt;

        }
        else {
            //if (-e + inMin.x > 0.0f || -e + inMax.x < 0.0f) return std::nullopt;
        }
    }


    {
        glm::vec3 yaxis(inTransform[1]);
        float e = glm::dot(yaxis, delta);
        float f = glm::dot(m_Direction, yaxis);

        if (fabs(f) > 0.001f) {

            float t1 = (e + inOBB.mMin.y) / f;
            float t2 = (e + inOBB.mMax.y) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < t_max) t_max = t2;
            if (t1 > t_min) t_min = t1;
            if (t_min > t_max) return std::nullopt;

        }
        else {
            //if (-e + inMin.y > 0.0f || -e + inMax.y < 0.0f) return std::nullopt;
        }
    }


    {
        glm::vec3 zaxis(inTransform[2]);
        float e = glm::dot(zaxis, delta);
        float f = glm::dot(m_Direction, zaxis);

        if (fabs(f) > 0.001f) {

            float t1 = (e + inOBB.mMin.z) / f;
            float t2 = (e + inOBB.mMax.z) / f;

            if (t1 > t2) std::swap(t1, t2);

            if (t2 < t_max) t_max = t2;
            if (t1 > t_min) t_min = t1;
            if (t_min > t_max) return std::nullopt;

        }
        else {
            //if (-e + inMin.z > 0.0f || -e + inMax.z < 0.0f) return std::nullopt;
        }
    }

    return t_min;
}


std::optional<float> Ray::HitsAABB(const BBox3D& inAABB) const {
    auto t1 = (inAABB.mMin.x - m_Origin.x) * m_RcpDirection.x;
    auto t2 = (inAABB.mMax.x - m_Origin.x) * m_RcpDirection.x;

    auto tmin = glm::min(t1, t2);
    auto tmax = glm::max(t1, t2);

    for (int i = 1; i < 3; i++) {
        t1 = (inAABB.mMin[i] - m_Origin[i]) * m_RcpDirection[i];
        t1 = (inAABB.mMax[i] - m_Origin[i]) * m_RcpDirection[i];

        tmin = glm::max(tmin, glm::min(glm::min(t1, t2), tmax));
        tmax = glm::min(tmax, glm::max(glm::max(t1, t2), tmin));
    }

    return tmax > glm::max(tmin, 0.0f) ? std::make_optional(tmax) : std::nullopt;
}


std::optional<float> Ray::HitsTriangle(const Vec3& inV0, const Vec3& inV1, const Vec3& inV2) const {
    auto p1 = inV1 - inV0;
    auto p2 = inV2 - inV0;
    auto pvec = glm::cross(m_Direction, p2);
    float det = glm::dot(p1, pvec);

    if (fabs(det) < std::numeric_limits<float>::epsilon()) return std::nullopt;

    float invDet = 1 / det;
    auto tvec = m_Origin - inV0;
    auto u = glm::dot(tvec, pvec) * invDet;
    if (u < 0 || u > 1) return std::nullopt;

    auto qvec = glm::cross(tvec, p1);
    auto v = glm::dot(m_Direction, qvec) * invDet;
    if (v < 0 || u + v > 1) return std::nullopt;

    return glm::dot(p2, qvec) * invDet;
}


std::optional<float> Ray::HitsSphere(const Vec3& inPosition, float inRadius, float inTmin, float inTmax) const {
    auto R2 = inRadius * inRadius;
    glm::vec3 L = inPosition - m_Origin;
    float tca = glm::dot(L, glm::normalize(m_Direction));
    if (tca < 0) return std::nullopt;

    float D2 = dot(L, L) - tca * tca;
    if (D2 > R2) return std::nullopt;
    float thc = sqrt(R2 - D2);
    float t0 = tca - thc;
    float t1 = tca + thc;

    float closest_t = std::min(t0, t1);

    if (closest_t < inTmax && closest_t > inTmin) {
        return closest_t / glm::length(m_Direction);
    }

    return std::nullopt;
}


bool gPointInAABB(const Vec3& inPoint, const BBox3D& inAABB) {
    return (inPoint.x >= inAABB.mMin.x && inPoint.x <= inAABB.mMax.x) &&
           (inPoint.y >= inAABB.mMin.y && inPoint.y <= inAABB.mMax.y) &&
           (inPoint.z >= inAABB.mMin.z && inPoint.z <= inAABB.mMax.z);
}


float gRandomFloat01() {
    return sUniformDistribution01(sDefaultRandomEngine);
}


 /* "Fast Random Rotation Matrices" - James Arvo, Graphics Gems 3 */
Mat3x3 gRandomRotationMatrix() {
    float x[3] = { gRandomFloat01(), gRandomFloat01(), gRandomFloat01() };
    
    constexpr float PITIMES2 = M_PI * 2;
    float theta = x[0] * PITIMES2;  /* Rotation about the pole (Z).      */
    float phi = x[1] * PITIMES2;    /* For direction of pole deflection. */
    float z = x[2] * 2.f;           /* For magnitude of pole deflection. */

    /* Compute a vector V used for distributing points over the sphere  */
    /* via the reflection I - V Transpose(V).  This formulation of V    */
    /* will guarantee that if x[1] and x[2] are uniformly distributed,  */
    /* the reflected points will be uniform on the sphere.  Note that V */
    /* has length sqrt(2) to eliminate the 2 in the Householder matrix. */

    float r = sqrtf(z);
    float Vx = sinf(phi) * r;
    float Vy = cosf(phi) * r;
    float Vz = sqrtf(2.f - z);

    /* Compute the row vector S = Transpose(V) * R, where R is a simple */
    /* rotation by theta about the z-axis.  No need to compute Sz since */
    /* it's just Vz.                                                    */

    float st = sinf(theta);
    float ct = cosf(theta);
    float Sx = Vx * ct - Vy * st;
    float Sy = Vx * st + Vy * ct;

    /* Construct the rotation matrix  ( V Transpose(V) - I ) R, which   */
    /* is equivalent to V S - R.                                        */

    auto matrix = Mat3x3(1.0f);

    matrix[0][0] = Vx * Sx - ct;
    matrix[0][1] = Vx * Sy - st;
    matrix[0][2] = Vx * Vz;

    matrix[1][0] = Vy * Sx + st;
    matrix[1][1] = Vy * Sy - ct;
    matrix[1][2] = Vy * Vz;

    matrix[2][0] = Vz * Sx;
    matrix[2][1] = Vz * Sy;
    matrix[2][2] = 1.0 - z;   /* This equals Vz * Vz - 1.0 */

    return glm::transpose(matrix);
}


/*
    Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix by G. Gribb & K. Hartmann 
    https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
*/
Frustum::Frustum(const glm::mat4& inViewProjMatrix, bool inShouldNormalize) {
    m_Planes[0].x = inViewProjMatrix[0][3] + inViewProjMatrix[0][0];
    m_Planes[0].y = inViewProjMatrix[1][3] + inViewProjMatrix[1][0];
    m_Planes[0].z = inViewProjMatrix[2][3] + inViewProjMatrix[2][0];
    m_Planes[0].w = inViewProjMatrix[3][3] + inViewProjMatrix[3][0];

    m_Planes[1].x = inViewProjMatrix[0][3] - inViewProjMatrix[0][0];
    m_Planes[1].y = inViewProjMatrix[1][3] - inViewProjMatrix[1][0];
    m_Planes[1].z = inViewProjMatrix[2][3] - inViewProjMatrix[2][0];
    m_Planes[1].w = inViewProjMatrix[3][3] - inViewProjMatrix[3][0];

    m_Planes[2].x = inViewProjMatrix[0][3] - inViewProjMatrix[0][1];
    m_Planes[2].y = inViewProjMatrix[1][3] - inViewProjMatrix[1][1];
    m_Planes[2].z = inViewProjMatrix[2][3] - inViewProjMatrix[2][1];
    m_Planes[2].w = inViewProjMatrix[3][3] - inViewProjMatrix[3][1];

    m_Planes[3].x = inViewProjMatrix[0][3] + inViewProjMatrix[0][1];
    m_Planes[3].y = inViewProjMatrix[1][3] + inViewProjMatrix[1][1];
    m_Planes[3].z = inViewProjMatrix[2][3] + inViewProjMatrix[2][1];
    m_Planes[3].w = inViewProjMatrix[3][3] + inViewProjMatrix[3][1];

    m_Planes[4].x = inViewProjMatrix[0][3] + inViewProjMatrix[0][2];
    m_Planes[4].y = inViewProjMatrix[1][3] + inViewProjMatrix[1][2];
    m_Planes[4].z = inViewProjMatrix[2][3] + inViewProjMatrix[2][2];
    m_Planes[4].w = inViewProjMatrix[3][3] + inViewProjMatrix[3][2];

    m_Planes[5].x = inViewProjMatrix[0][3] - inViewProjMatrix[0][2];
    m_Planes[5].y = inViewProjMatrix[1][3] - inViewProjMatrix[1][2];
    m_Planes[5].z = inViewProjMatrix[2][3] - inViewProjMatrix[2][2];
    m_Planes[5].w = inViewProjMatrix[3][3] - inViewProjMatrix[3][2];

    if (inShouldNormalize) {
        for (auto& plane : m_Planes) {
            const auto mag = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
            plane.x = plane.x / mag;
            plane.y = plane.y / mag;
            plane.z = plane.z / mag;
            plane.w = plane.w / mag;
        }
    }
}


bool Frustum::ContainsAABB(const BBox3D& inAABB) const {
    for (const auto& plane : m_Planes) {
        int out = 0;
        out += ((glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMin.y, inAABB.mMin.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMin.y, inAABB.mMin.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMax.y, inAABB.mMin.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMax.y, inAABB.mMin.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMin.y, inAABB.mMax.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMin.y, inAABB.mMax.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMax.y, inAABB.mMax.z, 1.0f)) < 0.0) ? 1 : 0);
        out += ((glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMax.y, inAABB.mMax.z, 1.0f)) < 0.0) ? 1 : 0);

        if (out == 8) 
            return false;
    }

    return true;
}


} // raekor
