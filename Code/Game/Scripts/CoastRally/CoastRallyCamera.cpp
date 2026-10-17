#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

class LevelOnlyBodyFilter final : public JPH::BodyFilter
{
public:
    LevelOnlyBodyFilter(JPH::BodyID inLevel, JPH::BodyID inObstacles) : m_Level(inLevel), m_Obstacles(inObstacles) {}

    bool ShouldCollide(const JPH::BodyID& inBodyID) const override { return inBodyID == m_Level || inBodyID == m_Obstacles; }

private:
    JPH::BodyID m_Level;
    JPH::BodyID m_Obstacles;
};


void CoastRallyScript::SetCameraMode(int inMode)
{
    m_CameraMode = inMode;
    m_OrbitYaw = 0.0f;
    m_OrbitPitch = 0.0f;

    static constexpr StaticArray<const char*, CAMERA_MODE_COUNT> cNames = { "CHASE CAM", "FAR CAM", "HOOD CAM" };
    AddPopup(cNames[inMode], cCream);
}


float CoastRallyScript::CastCameraRay(const Vec3& inFrom, const Vec3& inTo)
{
    if (m_LevelBody.IsInvalid())
        return 1.0f;

    const JPH::RRayCast ray(ToJolt(inFrom), ToJolt(inTo - inFrom));
    JPH::RayCastResult hit;

    if (GetPhysicsSystem()->GetNarrowPhaseQuery().CastRay(ray, hit, { }, { }, LevelOnlyBodyFilter(m_LevelBody, m_ObstacleBody)))
        return hit.mFraction;

    return 1.0f;
}


void CoastRallyScript::UpdateCamera(float inDeltaTime)
{
    Transform* transform = FindComponent<Transform>(m_CameraEntity);
    Camera* camera = FindComponent<Camera>(m_CameraEntity);

    if (transform == nullptr || camera == nullptr)
        return;

    const float speed = GetForwardSpeed();
    const Vec3 forward = m_CarRotation * Vec3(0.0f, 0.0f, 1.0f);

    Vec3 position;
    Quat rotation;

    if (m_CameraMode == CAMERA_HOOD)
    {
        position = m_CarPosition + m_CarRotation * cHoodCameraOffset;
        rotation = glm::quatLookAtRH(forward, m_CarRotation * Vec3(0.0f, 1.0f, 0.0f));
        m_CameraYaw = GetYaw(forward);
    }
    else
    {
        const ChaseCamera& chase = cChaseCameras[m_CameraMode];
        const Vec3 flat_forward = Vec3(forward.x, 0.0f, forward.z);

        if (glm::length(flat_forward) > 0.2f)
            m_CameraYaw = LerpAngle(m_CameraYaw, GetYaw(flat_forward), 1.0f - glm::exp(-inDeltaTime * 3.5f));

        m_OrbitIdle += inDeltaTime;

        if (m_OrbitIdle > cOrbitReturnDelay)
        {
            m_OrbitYaw *= glm::exp(-inDeltaTime * 2.5f);
            m_OrbitPitch *= glm::exp(-inDeltaTime * 2.5f);
        }

        const float yaw = m_CameraYaw + m_OrbitYaw;
        const float pitch = glm::atan(chase.mHeight, chase.mDistance) + m_OrbitPitch;
        const float distance = glm::length(Vec2(chase.mDistance, chase.mHeight)) * ( 1.0f + glm::clamp(glm::abs(speed) / 50.0f, 0.0f, 1.0f) * 0.15f );

        const Vec3 pivot = m_CarPosition + Vec3(0.0f, chase.mLookHeight, 0.0f);
        const Vec3 direction = Vec3(-glm::sin(yaw) * glm::cos(pitch), glm::sin(pitch), -glm::cos(yaw) * glm::cos(pitch));
        const Vec3 desired = pivot + direction * distance;

        const float hit = CastCameraRay(pivot, desired);
        const float clear_distance = glm::max(distance * hit - cCameraClearance, 1.5f);

        const Vec3 target = pivot + direction * clear_distance;
        m_CameraPosition.x = target.x;
        m_CameraPosition.z = target.z;
        m_CameraPosition.y = glm::mix(m_CameraPosition.y, target.y, 1.0f - glm::exp(-inDeltaTime * 8.0f));

        position = m_CameraPosition;

        const Vec3 look_at = pivot + Vec3(glm::sin(yaw), 0.0f, glm::cos(yaw)) * 2.0f;
        rotation = glm::quatLookAtRH(glm::normalize(look_at - position), Camera::cUp);
    }

    m_Shake = glm::max(0.0f, m_Shake - inDeltaTime * 1.8f);

    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
    position += Vec3(jitter(m_Random), jitter(m_Random), jitter(m_Random)) * m_Shake * m_Shake * 0.25f;

    transform->position = position;
    transform->rotation = rotation;
    transform->Compose();

    const float target_fov = cChaseFov + cMaxFovBoost * glm::clamp(glm::abs(speed) / 45.0f, 0.0f, 1.0f) + ( m_Boosting ? 6.0f : 0.0f );
    m_Fov = glm::mix(m_Fov, target_fov, 1.0f - glm::exp(-inDeltaTime * 4.0f));
    camera->SetFov(m_CameraMode == CAMERA_HOOD ? m_Fov + 8.0f : m_Fov);
}

} // RK
