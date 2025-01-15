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
            settings.mShape = new JPH::CapsuleShape(0.5f, 0.25f);

            JPH::Vec3 pos = JPH::Vec3(0.0f, 2.0f, 0.0f);
            character = new JPH::Character(&settings, pos, JPH::Quat::sIdentity(), 0, GetPhysics()->GetSystem());
            character->AddToPhysicsSystem();
        }

        Transform& transform = GetComponent<Transform>();

        /*Vec3 forward = glm::normalize(glm::rotate(transform.rotation, Camera::cForward));
        Vec3 right = glm::normalize(glm::cross(forward, Camera::cUp));

        float y = transform.position.y;

        if (m_Input->IsKeyDown(Key::A))
            transform.position += right * m_Speed * inDeltaTime;

        if (m_Input->IsKeyDown(Key::D))
            transform.position -= right * m_Speed * inDeltaTime;

        if (m_Input->IsKeyDown(Key::W))
            transform.position -= forward * m_Speed * inDeltaTime;

        if (m_Input->IsKeyDown(Key::S))
            transform.position += forward * m_Speed * inDeltaTime;

        transform.position.y = y;

        transform.Compose();*/

        if (m_Input->IsKeyDown(Key::A))
            character->AddImpulse(JPH::Vec3Arg(1.0f, 0.0f, 0.0f));

        if (m_Input->IsKeyDown(Key::D))
            character->AddImpulse(JPH::Vec3Arg(-1.0f, 0.0f, 0.0f));


        if (m_Input->IsKeyDown(Key::W))
            character->AddImpulse(JPH::Vec3Arg(1.0f, 0.0f, 1.0f));


        if (m_Input->IsKeyDown(Key::S))
            character->AddImpulse(JPH::Vec3Arg(1.0f, 0.0f, -1.0f));


        JPH::Vec3 position;
        JPH::Quat rotation;
        character->GetPositionAndRotation(position, rotation);

        transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
        transform.rotation = glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());

        transform.Compose();
    }

    void OnEvent(const SDL_Event& inEvent) 
    {
        if (inEvent.type == SDL_MOUSEMOTION)
        {
            Vec2 motion = Vec2(inEvent.motion.xrel, inEvent.motion.yrel);
            
            Transform& transform = GetComponent<Transform>();

           
        }
    }

private:
    int m_Value = 12;
    float m_Speed = 1.0f;
    bool m_Enabled = true;
    Vec2 m_Angle = Vec2(0.0f);
    JPH::Character* character = nullptr;
    JPH::CharacterSettings settings;
};


RTTI_DEFINE_TYPE(TestScript) 
{
    RTTI_DEFINE_TYPE_INHERITANCE(TestScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF<bool>(), "Enabled", m_Enabled);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF<int>(), "Value", m_Value);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Speed", m_Speed);
}
