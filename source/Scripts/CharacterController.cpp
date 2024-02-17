#define RAEKOR_SCRIPT
#include "Raekor/raekor.h"
using namespace Raekor;


class CharacterControllerScript : public INativeScript 
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(CharacterControllerScript);

    void OnStart() override
    {
        m_Input->SetRelativeMouseMode(true);

        m_Offset = Vec3(-m_Distance, m_Height, 0.0f);
    }

    void OnUpdate(float inDeltaTime) override
    {
        Transform& player_transform = GetComponent<Transform>();

        /// HANDLE INPUT
        
        Vec3 right_vector = glm::normalize(glm::cross(m_Direction, Vec3(0.0f, 1.0f, 0.0f)));

        if (m_Input->IsKeyPressed(SDL_SCANCODE_W)) 
        {
            m_Velocity += m_Direction * ( m_Speed / m_Mass ) * inDeltaTime;

        }

        if (m_Input->IsKeyPressed(SDL_SCANCODE_S))
        {
            m_Velocity -= m_Direction * ( m_Speed / m_Mass ) * inDeltaTime;

        }

        if (m_Input->IsKeyPressed(SDL_SCANCODE_A))
        {


            m_Velocity -= right_vector * ( m_Speed / m_Mass ) * inDeltaTime;
        }

        if (m_Input->IsKeyPressed(SDL_SCANCODE_D))
        {

            m_Velocity += right_vector * ( m_Speed / m_Mass ) * inDeltaTime;
        }

        player_transform.position += m_Velocity;



        if (Skeleton* player_skeleton = TryGetComponent<Skeleton>())
        {
            if (glm::all(glm::lessThan(m_Velocity, Vec3(FLT_EPSILON * 2.0f))))
                player_skeleton->animationIndex = -1;
            else
                player_skeleton->animationIndex = 0;
        }

        m_Velocity = m_Velocity * m_Mass * inDeltaTime;
        player_transform.position.y = glm::max(player_transform.position.y - m_Gravity * inDeltaTime, 0.0f);

        player_transform.Compose();

        m_DebugRenderer->AddLine(player_transform.position, player_transform.position + m_Direction * 3.0f);
        m_DebugRenderer->AddLine(player_transform.position, player_transform.position + right_vector * 3.0f);

        /// HANDLE CAMERA ORBIT

        if (m_Follow)
        {
            // Calculate the desired position of the camera
            Vec3 player_position = player_transform.position;
            player_position.y += m_PlayerHeight;

            Vec3 desired_position = player_position + m_Offset;

            // Perform smooth damping to the desired position
            m_Camera->SetPosition(glm::lerp(m_Camera->GetPosition(), desired_position, inDeltaTime * 1.5f));

            // Rotate the camera to look at the player
            m_Camera->LookAt(-glm::normalize(m_Camera->GetPosition() - player_position + Vec3(0.0f, 1.0f, 0.0f) * m_Height));
        }
    }

    void OnEvent(const SDL_Event& inEvent) 
    {
        if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
        {
            switch (inEvent.key.keysym.sym)
            {
                case SDLK_SPACE:
                {
                    m_Velocity.y += m_JumpHeight;
                } break;
            }
        }

        if (inEvent.type == SDL_MOUSEMOTION)
        {
        }
    }

    void OnStop() override
    {
        m_Input->SetRelativeMouseMode(false);
    }

private:
    bool m_Follow = true;

    float m_Mass = 0.5f;
    float m_Speed = 1.0f;
    float m_Height = 1.0f;
    float m_Distance = 2.0f;
    float m_JumpHeight = 2.0f;
    float m_PlayerHeight = 1.5f;

private:
    float m_Gravity = 9.81f;
    Vec3 m_Offset = Vec3(0, 0, 0);
    Vec3 m_Velocity = Vec3(0, 0, 0);
    Vec3 m_Direction = Vec3(1, 0, 0);
    Vec2 m_CameraAngle = Vec2(0, 0);
};


RTTI_DEFINE_TYPE(CharacterControllerScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(CharacterControllerScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF(bool), "Camera Follow", m_Follow);

    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF(float), "Mass", m_Mass);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF(float), "Speed", m_Speed);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF(float), "Height", m_Height);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF(float), "Distance", m_Distance);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF(float), "Jump Height", m_JumpHeight);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF(float), "Player Height", m_PlayerHeight);
}