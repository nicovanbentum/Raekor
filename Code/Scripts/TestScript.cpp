#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"
using namespace RK;

#include "JoltPhysics/Jolt/Physics/Character/Character.h"
#include "JoltPhysics/Jolt/Physics/Collision/Shape/CapsuleShape.h"

class TestScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(TestScript);

    void OnUpdate(float inDeltaTime) override
    {
        if (!m_Enabled)
            return;

        if (character == nullptr)
        {
            // Initialize character settings and character object
            settings.mShape = new JPH::CapsuleShape(0.5f, 0.25f);
            JPH::Vec3 startPosition = JPH::Vec3(0.0f, 2.0f, 0.0f);

            character = new JPH::Character(
                &settings,
                startPosition,
                JPH::Quat::sIdentity(),
                0, // Object layer
                GetPhysics()->GetSystem()
            );
            character->AddToPhysicsSystem();
        }

        Transform& transform = GetComponent<Transform>();

        // Movement logic (not rotation-specific)
        Vec3 forward = glm::normalize(glm::rotate(transform.rotation, Camera::cForward));
        forward.y = 0.0f;

        Vec3 right = glm::normalize(glm::cross(forward, Camera::cUp));

        Vec3 movement(0.0f);
        if (m_Input->IsKeyDown(Key::W))
            movement += forward;
        if (m_Input->IsKeyDown(Key::S))
            movement -= forward;
        if (m_Input->IsKeyDown(Key::A))
            movement -= right;
        if (m_Input->IsKeyDown(Key::D))
            movement += right;

        if (glm::length(movement) > 0.0f)
            movement = glm::normalize(movement) * m_Speed;

        // Set character's velocity
        JPH::Vec3 old_velocity = character->GetLinearVelocity();
        JPH::Vec3 new_velocity = JPH::Vec3(movement.x, old_velocity.GetY(), movement.z);
        character->SetLinearVelocity(new_velocity);
#if 0
        // Apply transform's rotation to the character body
        JPH::Quat bodyRotation = JPH::Quat(
            transform.rotation.x,
            transform.rotation.y,
            transform.rotation.z,
            transform.rotation.w
        );
        character->SetRotation(bodyRotation);
#endif
        // Update transform position to match character body
        JPH::Vec3 position = character->GetPosition();
        transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
        transform.Compose();

        transform.rotation = glm::quatLookAt(m_Camera.GetForward(), Camera::cUp);
        transform.Compose();
    }

    void OnEvent(const SDL_Event& inEvent) override
    {
        if (inEvent.type == SDL_MOUSEMOTION)
        {
            const CVar& sens_cvar = g_CVariables->GetCVar("sensitivity");
            const float formula = glm::radians(0.022f * sens_cvar.mFloatValue * 2.0f);
            m_Camera.Look(Vec2(inEvent.motion.xrel, inEvent.motion.yrel) * formula);
        }

        if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
        {
            switch (inEvent.key.keysym.sym)
            {
                case SDLK_SPACE:
                {
                    JPH::Vec3 velocity = character->GetLinearVelocity();
                    character->SetLinearVelocity(JPH::Vec3(velocity.GetX(), m_JumpHeight, velocity.GetZ()));
                } break;
            }
        }
    }


    ~TestScript()
    {
        // Cleanup allocated resources
        if (character)
        {
            character->RemoveFromPhysicsSystem();
            delete character;
            character = nullptr;
        }

        if (settings.mShape)
        {
            delete settings.mShape;
            settings.mShape = nullptr;
        }
    }

private:
    int m_Value = 12;
    float m_Speed = 2.5f;                   // Increased speed for more responsive movement
    float m_JumpHeight = 2.0f;
    bool m_Enabled = true;
    Camera m_Camera;
    glm::quat m_CameraRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Separate camera rotation
    JPH::Character* character = nullptr;
    JPH::CharacterSettings settings;
};



RTTI_DEFINE_TYPE(TestScript) 
{
    RTTI_DEFINE_TYPE_INHERITANCE(TestScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF<bool>(), "Enabled", m_Enabled);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF<int>(), "Value", m_Value);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Speed", m_Speed);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Jump Height", m_JumpHeight);
}
