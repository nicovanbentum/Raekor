#define RAEKOR_SCRIPT
#include "Raekor/raekor.h"
using namespace RK;


class VehicleControllerScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(VehicleControllerScript);

public:
    static constexpr float  cMaxRotationSpeed = 10.0f * M_PI;
    static constexpr float	cMaxSteeringAngle = glm::radians(30.0f);

    using EAxis = JPH::SixDOFConstraintSettings::EAxis;

    enum class EWheel : int
    {
        LeftFront,
        RightFront,
        LeftRear,
        RightRear,
        Num,
    };

    static inline bool sIsFrontWheel(EWheel inWheel) { return inWheel == EWheel::LeftFront || inWheel == EWheel::RightFront; }
    static inline bool sIsLeftWheel(EWheel inWheel) { return inWheel == EWheel::LeftFront || inWheel == EWheel::LeftRear; }

    void OnStart() override
    {
        m_WheelEntities[(int)EWheel::LeftFront]  = m_WheelEntityFL;
        m_WheelEntities[(int)EWheel::RightFront] = m_WheelEntityFR;
        m_WheelEntities[(int)EWheel::LeftRear]   = m_WheelEntityBL;
        m_WheelEntities[(int)EWheel::RightRear]  = m_WheelEntityBR;

        RigidBody* body_collider = FindComponent<RigidBody>(m_BodyEntity);

        for (Entity wheel_entity : m_WheelEntities)
        {
            if (wheel_entity == Entity::Null)
            {
                Log("Wheel has no Entity!");
                return;
            }
        }

        JPH::BodyInterface& body_interface = GetPhysics()->GetSystem()->GetBodyInterface();

        // Create wheels
        for (int i = 0; i < (int)EWheel::Num; ++i)
        {
            RigidBody* wheel_collider = FindComponent<RigidBody>(m_WheelEntities[i]);
            assert(wheel_collider);

            bool is_front = sIsFrontWheel((EWheel)i);
            bool is_left = sIsLeftWheel((EWheel)i);

            const Mesh* wheel_mesh = FindComponent<Mesh>(m_WheelEntities[i]);
            const Transform* wheel_transform = FindComponent<Transform>(m_WheelEntities[i]);

            const Vec3 wheel_pos = wheel_transform->GetPositionWorldSpace();
            const BBox3D wheel_aabb = BBox3D(wheel_mesh->bbox);

            JPH::RVec3 wheel_pos1 = JPH::Vec3(wheel_pos.x, wheel_pos.y, wheel_pos.z);;
            JPH::RVec3 wheel_pos2 = wheel_pos1 - JPH::Vec3(0, wheel_aabb.GetHeight(), 0);

            // Create constraint
            JPH::SixDOFConstraintSettings settings;
            settings.mPosition1 = wheel_pos1;
            settings.mPosition2 = wheel_pos2;
            settings.mAxisX1 = settings.mAxisX2 = is_left ? -JPH::Vec3::sAxisX() : JPH::Vec3::sAxisX();
            settings.mAxisY1 = settings.mAxisY2 = JPH::Vec3::sAxisY();

            // The suspension works in the Y translation axis only
            settings.MakeFixedAxis(EAxis::TranslationX);
            settings.SetLimitedAxis(EAxis::TranslationY, -wheel_aabb.GetHeight(), wheel_aabb.GetHeight());
            settings.MakeFixedAxis(EAxis::TranslationZ);
            settings.mMotorSettings[EAxis::TranslationY] = JPH::MotorSettings(2.0f, 1.0f, 1.0e5f, 0.0f);

            // Front wheel can rotate around the Y axis
            if (is_front)
                settings.SetLimitedAxis(EAxis::RotationY, -cMaxSteeringAngle, cMaxSteeringAngle);
            else
                settings.MakeFixedAxis(EAxis::RotationY);

            // The Z axis is static
            settings.MakeFixedAxis(EAxis::RotationZ);

            // The main engine drives the X axis
            settings.MakeFreeAxis(EAxis::RotationX);
            settings.mMotorSettings[EAxis::RotationX] = JPH::MotorSettings(2.0f, 1.0f, 0.0f, 0.5e4f);

            // The front wheel needs to be able to steer around the Y axis
            // However the motors work in the constraint space of the wheel, and since this rotates around the 
            // X axis we need to drive both the Y and Z to steer
            if (is_front)
                settings.mMotorSettings[EAxis::RotationY] = settings.mMotorSettings[EAxis::RotationZ] = JPH::MotorSettings(10.0f, 1.0f, 0.0f, 1.0e6f);

            JPH::SixDOFConstraint* wheel_constraint = static_cast<JPH::SixDOFConstraint*>( body_interface.CreateConstraint(&settings, body_collider->bodyID, wheel_collider->bodyID) );
            GetPhysics()->GetSystem()->AddConstraint(wheel_constraint);
            m_WheelConstraints[i] = wheel_constraint;

            // Drive the suspension
            wheel_constraint->SetTargetPositionCS(JPH::Vec3(0, -wheel_aabb.GetHeight(), 0));
            wheel_constraint->SetMotorState(EAxis::TranslationY, JPH::EMotorState::Position);

            // The front wheels steer around the Y axis, but in constraint space of the wheel this means we need to drive
            // both Y and Z (see comment above)
            if (is_front)
            {
                wheel_constraint->SetTargetOrientationCS(JPH::Quat::sIdentity());
                wheel_constraint->SetMotorState(EAxis::RotationY, JPH::EMotorState::Position);
                wheel_constraint->SetMotorState(EAxis::RotationZ, JPH::EMotorState::Position);
            }
        }
    }

    void OnStop() override
    {

    }

    void OnUpdate(float inDeltaTime) override
    {
        RigidBody* body_collider = FindComponent<RigidBody>(m_BodyEntity);
        JPH::BodyInterface& body_interface = GetPhysics()->GetSystem()->GetBodyInterface();

        // Determine steering and speed
        float steering_angle = 0.0f, speed = 0.0f;
        if (m_Input->IsKeyPressed(SDL_SCANCODE_D)) 
            steering_angle = cMaxSteeringAngle;

        if (m_Input->IsKeyPressed(SDL_SCANCODE_A)) 
            steering_angle = -cMaxSteeringAngle;

        if (m_Input->IsKeyPressed(SDL_SCANCODE_W)) 
            speed = cMaxRotationSpeed;

        if (m_Input->IsKeyPressed(SDL_SCANCODE_S)) 
            speed = -cMaxRotationSpeed;

        // Brake if current velocity is in the opposite direction of the desired velocity
        JPH::Quat rotation = body_interface.GetRotation(body_collider->bodyID);
        JPH::Vec3 linear_velocity = body_interface.GetLinearVelocity(body_collider->bodyID);

        float car_speed = linear_velocity.Dot(rotation.RotateAxisZ());
        bool brake = speed != 0.0f && car_speed != 0.0f && glm::sign(speed) != glm::sign(car_speed);

        // Front wheels
        const EWheel front_wheels[] = { EWheel::LeftFront, EWheel::RightFront };
        for (EWheel w : front_wheels)
        {
            JPH::SixDOFConstraint* wheel_constraint = m_WheelConstraints[(int)w];
            assert(wheel_constraint);

            // Steer front wheels
            JPH::Quat steering_rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), steering_angle);
            wheel_constraint->SetTargetOrientationCS(steering_rotation);

            if (brake)
            {
                // Brake on front wheels
                wheel_constraint->SetTargetAngularVelocityCS(JPH::Vec3::sZero());
                wheel_constraint->SetMotorState(EAxis::RotationX, JPH::EMotorState::Velocity);
            }
            else if (speed != 0.0f)
            {
                // Front wheel drive, since the motors are applied in the constraint space of the wheel
                // it is always applied on the X axis
                wheel_constraint->SetTargetAngularVelocityCS(JPH::Vec3(sIsLeftWheel(w) ? -speed : speed, 0, 0));
                wheel_constraint->SetMotorState(EAxis::RotationX, JPH::EMotorState::Velocity);
            }
            else
            {
                // Free spin
                wheel_constraint->SetMotorState(EAxis::RotationX, JPH::EMotorState::Off);
            }
        }

        // Rear wheels
        const EWheel rear_wheels[] = { EWheel::LeftRear, EWheel::RightRear };
        for (EWheel w : rear_wheels)
        {
            JPH::SixDOFConstraint* wheel_constraint = m_WheelConstraints[(int)w];
            assert(wheel_constraint);

            if (brake)
            {
                // Brake on rear wheels
                wheel_constraint->SetTargetAngularVelocityCS(JPH::Vec3::sZero());
                wheel_constraint->SetMotorState(EAxis::RotationX, JPH::EMotorState::Velocity);
            }
            else
            {
                // Free spin
                wheel_constraint->SetMotorState(EAxis::RotationX, JPH::EMotorState::Off);
            }
        }
    }

    void OnEvent(const SDL_Event& inEvent) override
    {

    }

private:
    Entity m_BodyEntity;
    Entity m_WheelEntityFL;
    Entity m_WheelEntityFR;
    Entity m_WheelEntityBL;
    Entity m_WheelEntityBR;

    StaticArray<Entity, 4> m_WheelEntities = { Entity::Null, Entity::Null, Entity::Null, Entity::Null };
    StaticArray<JPH::Ref<JPH::SixDOFConstraint>, 4> m_WheelConstraints;
};


RTTI_DEFINE_TYPE(VehicleControllerScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(VehicleControllerScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(VehicleControllerScript, SERIALIZE_ALL, &RTTI_OF(Entity), "Body", m_BodyEntity);
    RTTI_DEFINE_SCRIPT_MEMBER(VehicleControllerScript, SERIALIZE_ALL, &RTTI_OF(Entity), "Wheel (FL)", m_WheelEntityFL);
    RTTI_DEFINE_SCRIPT_MEMBER(VehicleControllerScript, SERIALIZE_ALL, &RTTI_OF(Entity), "Wheel (FR)", m_WheelEntityFR);
    RTTI_DEFINE_SCRIPT_MEMBER(VehicleControllerScript, SERIALIZE_ALL, &RTTI_OF(Entity), "Wheel (BL)", m_WheelEntityBL);
    RTTI_DEFINE_SCRIPT_MEMBER(VehicleControllerScript, SERIALIZE_ALL, &RTTI_OF(Entity), "Wheel (BR)", m_WheelEntityBR);

    // Fields go here
}