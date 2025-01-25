#define RAEKOR_SCRIPT
#include "../Engine/raekor.h"
using namespace RK;


class CharacterControllerScript : public INativeScript 
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(CharacterControllerScript);

    void SetAnimation(Entity inEntity)
    {
        if (Animation* animation = FindComponent<Animation>(inEntity))
        {
            animation->SetIsPlaying(true);

            if (m_CurrentAnimationEntity != inEntity)
                animation->SetRunningTime(0.0f);
        }
        else return;

        for (Entity child : GetScene()->GetChildren(m_Entity))
        {
            if (Skeleton* skeleton = GetScene()->GetPtr<Skeleton>(child))
                skeleton->animation = inEntity;
        }

        m_CurrentAnimationEntity = inEntity;
    }

    void OnStart() override 
    {
    }

    void OnUpdate(float inDeltaTime) override
    {
        Transform& player_transform = GetComponent<Transform>();

        /// HANDLE INPUT
        
        // update player rotation incase any mouse input events changed it
        Vec3 cam_dir = Vec3(m_CameraDirection.x, 0.0f, m_CameraDirection.z);
        Vec3 right_vector = glm::normalize(glm::cross(cam_dir, Vec3(0.0f, 1.0f, 0.0f)));

        bool moved = false;
        Vec3 move_dir = Vec3(0, 0, 0);

        if (m_Input->IsKeyDown(Key::W))
        {
            moved = true;
            move_dir += cam_dir;
        }
        if (m_Input->IsKeyDown(Key::S))
        {
            moved = true;
            move_dir -= cam_dir;
        }
        if (m_Input->IsKeyDown(Key::A))
        {
            moved = true;
            move_dir -= right_vector;
        }
        if (m_Input->IsKeyDown(Key::D))
        {
            moved = true;
            move_dir += right_vector;
        }

        if (moved && glm::any(glm::epsilonNotEqual(move_dir, Vec3(0,0,0), FLT_EPSILON)))
        {
            m_PlayerDirection = glm::normalize(move_dir);
            m_Velocity += m_PlayerDirection * ( glm::max(m_Speed, 0.0001f) / m_Mass ) * inDeltaTime;
        }
        
        player_transform.rotation = glm::quatLookAt(-m_PlayerDirection, Vec3(0.0f, 1.0f, 0.0f));
        player_transform.position += m_Velocity;

        const bool is_moving = glm::any(glm::greaterThan(glm::abs(m_Velocity), Vec3(FLT_EPSILON * 2.0f)));

        Entity active_animation = m_IdleAnimationEntity;

        if (glm::any(glm::greaterThan(glm::abs(m_Velocity), Vec3(FLT_EPSILON * 2.0f))))
        {
            active_animation = glm::abs(m_Velocity.y) > 0.0 ? m_JumpAnimationEntity : m_RunAnimationEntity;
        }

        if (active_animation != Entity::Null)
            SetAnimation(active_animation);

        m_Velocity = m_Velocity * m_Mass * inDeltaTime;
        player_transform.position.y = glm::max(player_transform.position.y - m_Gravity * inDeltaTime, 0.0f);

        player_transform.Compose();

        //m_DebugRenderer->AddLine(player_transform.position, player_transform.position + m_CameraDirection * 3.0f);
        //m_DebugRenderer->AddLine(player_transform.position, player_transform.position + right_vector * 3.0f);

        /// HANDLE CAMERA ORBIT

        if (m_FollowCamera)
        {
            m_CameraOffset = Vec3(-m_CameraDistance, m_CameraHeight, 0.0f);

            Vec3 camera_pos = player_transform.position;
            camera_pos -= glm::normalize(m_CameraDirection) * m_CameraDistance;
            camera_pos.y += m_CameraHeight;

            // Calculate the desired position of the camera
            Vec3 player_position = player_transform.position;
            player_position.y += m_PlayerHeight;

            Vec3 desired_position = player_position + m_CameraOffset;

            // Perform smooth damping to the desired position
            //m_Camera->SetPosition(glm::lerp(m_Camera->GetPosition(), camera_pos, inDeltaTime * m_CameraSpeed));

            // Rotate the camera to look at the player
            //m_Camera->LookAt(-glm::normalize(m_Camera->GetPosition() - player_position + Vec3(0.0f, 1.0f, 0.0f) * m_CameraHeight));
        }
    }

    void OnEvent(const SDL_Event& inEvent) 
    {
        if (!m_Input->IsRelativeMouseMode())
            return;

        if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
        {
            switch (inEvent.key.keysym.sym)
            {
                case SDLK_SPACE:
                {
                    m_Velocity.y = sqrtf(2.0f * m_Gravity * m_JumpHeight);
                } break;
            }
        }

        if (inEvent.type == SDL_MOUSEMOTION)
        {
            Vec2 motion = Vec2(inEvent.motion.xrel, inEvent.motion.yrel);
            m_CameraDirection = glm::toMat3(glm::angleAxis(-2.0f * (motion.x / 1920.0f ), Vec3(0.0f, 1.0f, 0.0f))) * m_CameraDirection;
            // m_CameraDirection = glm::toMat3(glm::angleAxis(2.0f * (motion.y / 1080.0f), Vec3(0.0f, 0.0f, 1.0f))) * m_CameraDirection;
            m_CameraDirection = glm::normalize(m_CameraDirection);
        }
    }

    void OnStop() override {}

private:
    bool m_WasMoving = false;
    bool m_FollowCamera = true;
    Entity m_RunAnimationEntity = Entity::Null;
    Entity m_IdleAnimationEntity = Entity::Null;
    Entity m_JumpAnimationEntity = Entity::Null;
    Entity m_CurrentAnimationEntity = Entity::Null;

    float m_Test = 0.2f;
    float m_Mass = 0.3f;
    float m_Speed = 1.5f;
    float m_JumpHeight = 2.0f;
    float m_PlayerHeight = 2.775f;
    float m_CameraSpeed = 25.0f;
    float m_CameraHeight = 1.75f;
    float m_CameraDistance = 2.283f;

private:
    float m_Gravity = 9.81f;
    Vec3 m_Velocity = Vec3(0, 0, 0);
    Vec3 m_CameraDirection = Vec3(1, 0, 0);
    Vec3 m_PlayerDirection = Vec3(1, 0, 0);
    Vec3 m_CameraOffset = Vec3(0, 0, 0);
};


RTTI_DEFINE_TYPE(CharacterControllerScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(CharacterControllerScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<bool>(), "Follow Camera", m_FollowCamera);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<Entity>(), "Run Animation", m_RunAnimationEntity);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<Entity>(), "Idle Animation", m_IdleAnimationEntity);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<Entity>(), "Jump Animation", m_JumpAnimationEntity);

    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Test", m_Test);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Mass", m_Mass);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Run Speed", m_Speed);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Jump Height", m_JumpHeight);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Player Height", m_PlayerHeight);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Camera Speed", m_CameraSpeed);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Camera Height", m_CameraHeight);
    RTTI_DEFINE_SCRIPT_MEMBER(CharacterControllerScript, SERIALIZE_ALL, &RTTI_OF<float>(), "Camera Distance", m_CameraDistance);
}