#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"
using namespace RK;

#include "JoltPhysics/Jolt/Physics/Character/Character.h"
#include "JoltPhysics/Jolt/Physics/Collision/Shape/CapsuleShape.h"

static constexpr float cBulletSize = 0.1f;
static constexpr uint32_t cBulletCount = 5;

class GunScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(GunScript);

    void OnStart() override
    {
        m_Position = GetComponent<Transform>().position;

        if (m_SpotlightEntity == Entity::Null)
        {
            m_SpotlightEntity = m_Scene->CreateSpatialEntity("Flashlight");
            m_Scene->ParentTo(m_SpotlightEntity, GetEntity());

            if (Transform* transform = FindComponent<Transform>(m_SpotlightEntity))
            {
                transform->Translate(Vec3(0.5f, 0.0f, 0.0f));
                transform->Rotate(-90.0f, Camera::cUp);
                transform->Compose();
            }

            Light& light = m_Scene->Add<Light>(m_SpotlightEntity);
            light.type = LIGHT_TYPE_SPOT;
            light.color.a = 0.0f;
            light.attributes.x = 5.0f;
            light.attributes.z = 1.0f;
        }
    }

    void OnUpdate(float inDeltaTime) override
    {
        Transform& transform = GetComponent<Transform>();
        m_OffsetPosition = m_Position;

        float bob_speed = m_BobSpeed;
        if (m_Input->IsKeyDown(Key::LSHIFT))
            bob_speed *= 1.2f;

        // Sway
        if (m_EnableSway)
        {
            Vec2 invert_look = m_LookInput * -m_SwayStep;

            invert_look.x = glm::clamp(invert_look.x, -m_SwayMaxStepDistance, m_SwayMaxStepDistance);
            invert_look.y = glm::clamp(invert_look.y, -m_SwayMaxStepDistance, m_SwayMaxStepDistance);

            m_OffsetPosition.x += invert_look.x;
            m_OffsetPosition.y += invert_look.y;
        }

        //transform.position = glm::lerp(m_Position, m_SwayPosition, inDeltaTime * m_Speed);

        if (m_EnableBobbing)
        {
            if (RigidBody* rb = FindComponent<RigidBody>(m_PlayerEntity))
            {
                JPH::Vec3 velocity = m_App->GetPhysics()->GetSystem()->GetBodyInterface().GetLinearVelocity(rb->bodyID);
            }

            m_MoveInput = Vec2(1, 1);
            m_BobSpeedCurve += inDeltaTime * bob_speed;

            m_OffsetPosition.y += glm::sin(m_BobSpeedCurve) * m_BobLimit.y * (m_Input->IsKeyDown(Key::W) || m_Input->IsKeyDown(Key::S));
            m_OffsetPosition.x += glm::cos(m_BobSpeedCurve) * m_BobLimit.x * (m_Input->IsKeyDown(Key::A) || m_Input->IsKeyDown(Key::D));
        }

        transform.position = glm::lerp(transform.position, m_OffsetPosition, inDeltaTime);

        transform.Compose();

        m_LookInput = Vec2(0, 0);
    }

    void OnEvent(const SDL_Event& inEvent) override
    {
        if (inEvent.type == SDL_MOUSEMOTION)
        {
            const CVar& sens_cvar = g_CVariables->GetCVar("sensitivity");
            const float formula = glm::radians(0.022f * sens_cvar.mFloatValue);

            m_LookInput.x = inEvent.motion.xrel * formula;
            m_LookInput.y = inEvent.motion.yrel * formula;
        }

        if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
        {
            switch (inEvent.key.keysym.sym)
            {
                case SDLK_f:
                {
                    if (m_SpotlightEntity != Entity::Null)
                    {
                        if (Light* light = FindComponent<Light>(m_SpotlightEntity))
                        {
                            m_SpotLightEnabled = !m_SpotLightEnabled;
                            light->color.a = m_SpotLightEnabled ? 2.0f : 0.0f;
                        }
                    }
                } break;
            }
        }
    }


private:
    Entity m_PlayerEntity = Entity::Null;
    Entity m_SpotlightEntity = Entity::Null;

    bool m_SpotLightEnabled = false;

    float m_Speed = 1000.0f;
    bool m_EnableSway = true;
    bool m_EnableBobbing = true;
    
    float m_BobSpeed = 11.0f;
    float m_BobSpeedCurve = 0.0f;
    Vec2 m_BobLimit = Vec2(0.1, 0.1);

    float m_SwayStep = 0.2f;
    float m_SwayMaxStepDistance = 0.2f;

    Vec2 m_LookInput = Vec2(0, 0);
    Vec2 m_MoveInput = Vec2(0, 0);
    Vec3 m_Position = Vec3(0, 0, 0);
    Vec3 m_OffsetPosition = Vec3(0, 0, 0);
};



RTTI_DEFINE_TYPE(GunScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(GunScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, bool, "Enable Sway", m_EnableSway);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, bool, "Enable Bobbing", m_EnableBobbing);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Bob Speed", m_BobSpeed);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Sway Speed", m_Speed);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Sway Step", m_SwayStep);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Sway Distance", m_SwayMaxStepDistance);

}
