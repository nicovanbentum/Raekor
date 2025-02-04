#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"
using namespace RK;

#include "JoltPhysics/Jolt/Physics/Character/Character.h"
#include "JoltPhysics/Jolt/Physics/Collision/Shape/CapsuleShape.h"

static constexpr float cBulletSize = 0.1f;
static constexpr uint32_t cBulletCount = 5;

class TestScript : public INativeScript
{
public:
    TestScript()
    {
        for (Entity& entity : m_Bullets)
            entity = Entity::Null;
    }

    RTTI_DECLARE_VIRTUAL_TYPE(TestScript);

    void OnStart() override
    {
        // active camera = camera component of this entity!
        if (m_Character == nullptr)
        {
            // Initialize character settings and character object
            settings.mShape = new JPH::CapsuleShape(0.85f, 0.25f);

            Transform& transform = GetComponent<Transform>();
            JPH::Vec3 pos = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);

            m_Character = new JPH::Character(
                &settings,
                pos,
                JPH::Quat::sIdentity(),
                0, // Object layer
                GetPhysics()->GetSystem()
            );
            m_Character->AddToPhysicsSystem();

            if (RigidBody* rb = FindComponent<RigidBody>())
            {
                rb->bodyID = m_Character->GetBodyID();
                rb->motion = RigidBody::DYNAMIC;
            }
        }

#if 1
        for (Entity& entity : m_Bullets)
        {
            if (entity == Entity::Null)
            {
                entity = m_Scene->CreateSpatialEntity("Bullet");
                Mesh& mesh = m_Scene->Add<Mesh>(entity);
                mesh.CreateSphere(mesh, cBulletSize, 16, 16);
                m_App->GetRenderInterface()->UploadMeshBuffers(entity, mesh);
                
                RigidBody& rb = m_Scene->Add<RigidBody>(entity);
                rb.motionType = JPH::EMotionType::Dynamic;
                rb.CreateSphereCollider(*GetPhysics(), cBulletSize);
                rb.CreateBody(*GetPhysics(), *FindComponent<Transform>(entity));

                Light& light = m_Scene->Add<Light>(entity);
                light.type = LIGHT_TYPE_POINT;
                light.color = Vec4(1.0f, 0.6f, 0.0f, 0.02f);

                if (Material* material = FindComponent<Material>(m_BulletMaterial))
                {
                    mesh.material = m_BulletMaterial;
                }
                else
                {
                    Entity e = m_Scene->Create();
                    Material& m = m_Scene->Add<Material>(e);

                    m.albedo = Vec4(1.0f, 0.6f, 0.0f, 1.0f);
                    m.emissive = Vec4(2.0f, 1.2f, 0.0f, 1.0f);

                    mesh.material = e;
                    m_BulletMaterial = e;
                }
            }
        }
#endif
    }

    void OnUpdate(float inDeltaTime) override
    {
        m_Character->PostSimulation(0.01f);

        Transform& transform = GetComponent<Transform>();

        // Movement logic (not rotation-specific)
        Vec3 forward = glm::normalize(glm::rotate(transform.rotation, Camera::cForward));
        forward.y = 0.0f;

        Vec3 right = glm::normalize(glm::cross(forward, Camera::cUp));

        Vec3 movement(0.0f);
        if (m_Character->GetGroundState() == JPH::Character::EGroundState::InAir)
        {
            forward = Vec3(0.0f);
            right = right * 0.2f;
        }

        if (m_Input->IsKeyDown(Key::W))
            movement += forward;
        
        if (m_Input->IsKeyDown(Key::S))
            movement -= forward;

        if (m_Input->IsKeyDown(Key::A))
            movement -= right;
        
        if (m_Input->IsKeyDown(Key::D))
            movement += right;

        if (glm::length(movement) > 0.0f)
        {
            movement = glm::normalize(movement) * m_Speed;

            if (m_Input->IsKeyDown(Key::LSHIFT) && m_Character->GetGroundState() != JPH::Character::EGroundState::InAir)
            {
                movement *= 1.3f;
            }
        }

        // Set character's velocity
        if (m_Character->GetGroundState() != JPH::Character::EGroundState::InAir)
        {
            JPH::Vec3 old_velocity = m_Character->GetLinearVelocity();
            JPH::Vec3 new_velocity = JPH::Vec3(movement.x, old_velocity.GetY(), movement.z);
            m_Character->SetLinearVelocity(new_velocity);
        }
        else
        {
        }

        // Update transform position to match character body
        JPH::Vec3 position = m_Character->GetPosition();
        transform.position = Vec3(position.GetX(), position.GetY() + m_HeightOffset, position.GetZ());
        transform.rotation = glm::quatLookAt(m_Camera.GetForward(), Camera::cUp);
        transform.Compose();
    }

    void OnEvent(const SDL_Event& inEvent) override
    {
        if (inEvent.type == SDL_MOUSEMOTION)
        {
            const CVar& sens_cvar = g_CVariables->GetCVar("sensitivity");
            const float formula = glm::radians(0.022f * sens_cvar.mFloatValue);
            m_Camera.Look(Vec2(inEvent.motion.xrel, inEvent.motion.yrel) * formula);
        }

        if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
        {
            switch (inEvent.key.keysym.sym)
            {
                case SDLK_SPACE:
                {
                    if (m_Character->GetGroundState() != JPH::Character::EGroundState::InAir)
                    {
                        JPH::Vec3 velocity = m_Character->GetLinearVelocity();
                        m_Character->SetLinearVelocity(JPH::Vec3(velocity.GetX(), m_JumpHeight, velocity.GetZ()));
                    }
                } break;
            }
        }

        if (inEvent.type == SDL_MOUSEBUTTONDOWN && inEvent.button.button == 1)
        {
            if (RigidBody* rb = FindComponent<RigidBody>(m_Bullets[m_BulletIndex]))
            {
                if (GetPhysics()->GetSystem()->GetBodyInterface().IsAdded(rb->bodyID))
                    rb->DeactivateBody(*GetPhysics());

                Vec3 fwd = m_Camera.GetForward();

                Transform transform = GetComponent<Transform>();
                transform.position += fwd * 0.30f;
                rb->ActivateBody(*GetPhysics(), transform);
                
                Vec3 impulse = fwd * m_BulletVelocity;
                GetPhysics()->GetSystem()->GetBodyInterface().AddImpulse(rb->bodyID, JPH::Vec3(impulse.x, impulse.y, impulse.z));

                m_BulletIndex = (m_BulletIndex + 1) % m_Bullets.size();
            }
        }
    }


    ~TestScript()
    {
        if (m_Character)
        {
            m_Character->RemoveFromPhysicsSystem();
            delete m_Character;
            m_Character = nullptr;
        }

        if (settings.mShape)
        {
            delete settings.mShape;
            settings.mShape = nullptr;
        }
    }

private:
    float m_Speed = 2.5f;
    float m_JumpHeight = 5.0f;
    float m_HeightOffset = 0.65f;
    
    int m_BulletIndex = 0;
    float m_BulletVelocity = 100.0f;
    Entity m_BulletMaterial = Entity::Null;
    StaticArray<Entity, cBulletCount> m_Bullets;

    float m_SwayStep = 0.01f;
    float m_SwayMaxStepDistance = 0.06f;
    Vec3 m_SwayPosition = Vec3(0, 0, 0);

    Camera m_Camera;
    Vec2 m_LookInput = Vec2(0, 0);
    float m_CameraFov = 65.0f;
    JPH::Character* m_Character = nullptr;
    JPH::CharacterSettings settings;
};



RTTI_DEFINE_TYPE(TestScript) 
{
    RTTI_DEFINE_TYPE_INHERITANCE(TestScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, float, "Speed", m_Speed);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, float, "Jump Height", m_JumpHeight);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, float, "Height Offset", m_HeightOffset);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, float, "Bullet Velocity", m_BulletVelocity);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, Entity, "Bullet Material", m_BulletMaterial);
}                                                        
