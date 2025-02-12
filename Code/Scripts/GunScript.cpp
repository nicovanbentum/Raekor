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

    void DrawCrosshair(float inDeltaTime)
    {
        const Viewport& viewport = m_App->GetViewport();
        Vec2 screen_center = Vec2(viewport.GetDisplaySize()) / 2.0f;

        //g_UIRenderer.AddCircleFilled(screen_center, m_CrosshairSize, Vec4(1.0f));

        float speed = 0.0f;

        if (RigidBody* rb = FindComponent<RigidBody>(m_Scene->GetParent(GetEntity())))
        {
            speed = m_App->GetPhysics()->GetSystem()->GetBodyInterface().GetLinearVelocity(rb->bodyID).Length();
        }

        //bool is_moving_forward = m_Input->IsKeyDown(Key::W) || m_Input->IsKeyDown(Key::S);
        //bool is_moving_sideways = m_Input->IsKeyDown(Key::A) || m_Input->IsKeyDown(Key::D);

        //float gap_multiplier = g_Input->IsKeyDown(Key::LSHIFT) ? 2.5f : 2.0f;
        float gap_multiplier = glm::max(glm::abs(speed), 1.0f);

        if (speed > 0.0f)
            m_CurrentCrosshairGap = glm::lerp(m_CurrentCrosshairGap, m_CrosshairGap * gap_multiplier, inDeltaTime * 16.666f);
        else
            m_CurrentCrosshairGap = glm::lerp(m_CurrentCrosshairGap, m_CrosshairGap, inDeltaTime * 16.666f);

        g_UIRenderer.AddRectFilled(screen_center + Vec2(0, m_CurrentCrosshairGap), Vec2(m_CrosshairThickness, m_CrosshairSize), 0.0f, Vec4(1.0f));
        g_UIRenderer.AddRectFilled(screen_center - Vec2(0, m_CurrentCrosshairGap), Vec2(m_CrosshairThickness, m_CrosshairSize), 0.0f, Vec4(1.0f));
        g_UIRenderer.AddRectFilled(screen_center + Vec2(m_CurrentCrosshairGap, 0), Vec2(m_CrosshairSize, m_CrosshairThickness), 0.0f, Vec4(1.0f));
        g_UIRenderer.AddRectFilled(screen_center - Vec2(m_CurrentCrosshairGap, 0), Vec2(m_CrosshairSize, m_CrosshairThickness), 0.0f, Vec4(1.0f));
    }

    void OnStart() override
    {
        m_Position = GetComponent<Transform>().position;
        m_CurrentCrosshairGap = m_CrosshairGap;

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

        DrawCrosshair(inDeltaTime);

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
    // entities
    Entity m_PlayerEntity = Entity::Null;
    Entity m_SpotlightEntity = Entity::Null;

    // runtime state
    bool m_IsMoving;
    float m_CurrentCrosshairGap;
    bool m_SpotLightEnabled = false;
    
    Vec2 m_LookInput = Vec2(0, 0);
    Vec2 m_MoveInput = Vec2(0, 0);
    Vec3 m_Position = Vec3(0, 0, 0);
    Vec3 m_OffsetPosition = Vec3(0, 0, 0);
    
    // crosshair settings
    float m_CrosshairGap = 8.0f;
    float m_CrosshairSize = 4.0f;
    float m_CrosshairThickness = 1.0f;

    // sway settings
    float m_SwayStep = 0.2f;
    float m_SwaySpeed = 1000.0f;
    float m_SwayMaxStepDistance = 0.2f;
    bool m_EnableSway = true;
    bool m_EnableBobbing = true;
    
    // bob settings
    float m_BobSpeed = 11.0f;
    float m_BobSpeedCurve = 0.0f;
    Vec2 m_BobLimit = Vec2(0.1, 0.1);


};



RTTI_DEFINE_TYPE(GunScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(GunScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, Entity, "Player Entity", m_PlayerEntity);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Crosshair Gap", m_CrosshairGap);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Crosshair Size", m_CrosshairSize);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Crosshair Thickness", m_CrosshairThickness);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, bool, "Enable Sway", m_EnableSway);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, bool, "Enable Bobbing", m_EnableBobbing);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Bob Speed", m_BobSpeed);

    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Sway Speed", m_SwaySpeed);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Sway Step", m_SwayStep);
    RTTI_DEFINE_SCRIPT_MEMBER(GunScript, SERIALIZE_ALL, float, "Sway Distance", m_SwayMaxStepDistance);

}
