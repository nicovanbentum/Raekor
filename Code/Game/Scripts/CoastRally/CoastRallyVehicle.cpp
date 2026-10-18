#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

void CoastRallyScript::CreateCar()
{
    m_CarModel = CreateModel("Car");
    m_BrakeNode = FindPart(m_CarModel, "Brake");

    for (Node& wheel : m_WheelNodes)
        wheel = CreateModel("Wheel").mRoot;

    for (Entity& entity : m_HeadlightEntities)
    {
        entity = CreateEntity("Headlight");

        Light& light = m_Scene->Add<Light>(entity);
        light.type = LIGHT_TYPE_SPOT;
        light.color = Vec4(cHeadlightColor, 0.0f);
        light.attributes = Vec4(cHeadlightRange, cHeadlightInnerAngle, cHeadlightOuterAngle, 0.0f);
    }

    JPH::PhysicsSystem* system = GetPhysicsSystem();
    JPH::BodyInterface& bodies = system->GetBodyInterface();

    const JPH::Vec3 center_of_mass = JPH::Vec3(0.0f, cCenterOfMassHeight, 0.0f);
    const JPH::RefConst<JPH::Shape> shape = JPH::OffsetCenterOfMassShapeSettings(center_of_mass, new JPH::BoxShape(ToJolt(cCarHalfExtents))).Create().Get();

    JPH::BodyCreationSettings body_settings(shape, ToJolt(m_Spawn.mPosition + Vec3(0.0f, cSpawnHeight, 0.0f)), ToJolt(m_Spawn.mRotation), JPH::EMotionType::Dynamic, EPhysicsObjectLayers::MOVING);
    body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    body_settings.mMassPropertiesOverride.mMass = cCarMass;
    body_settings.mFriction = 0.4f;
    body_settings.mCollisionGroup = GetCollisionGroup(COLLISION_SUBGROUP_CAR);

    JPH::Body* body = bodies.CreateBody(body_settings);

    if (body == nullptr)
    {
        Log("Coast Rally: failed to create the car body");
        return;
    }

    m_CarBody = body->GetID();
    bodies.AddBody(m_CarBody, JPH::EActivation::Activate);

    JPH::VehicleConstraintSettings vehicle;
    vehicle.mMaxPitchRollAngle = cMaxPitchRollAngle;

    const StaticArray<Vec3, WHEEL_COUNT> wheel_positions =
    {
        Vec3(cWheelOffset.x, cWheelOffset.y, cFrontWheelZ),
        Vec3(-cWheelOffset.x, cWheelOffset.y, cFrontWheelZ),
        Vec3(cWheelOffset.x, cWheelOffset.y, cRearWheelZ),
        Vec3(-cWheelOffset.x, cWheelOffset.y, cRearWheelZ),
    };

    for (int index = 0; index < WHEEL_COUNT; index++)
    {
        const bool front = index == WHEEL_FRONT_LEFT || index == WHEEL_FRONT_RIGHT;

        JPH::WheelSettingsWV* wheel = new JPH::WheelSettingsWV;
        wheel->mPosition = ToJolt(wheel_positions[index]);
        wheel->mMaxSteerAngle = front ? cMaxSteerAngle : 0.0f;
        wheel->mMaxHandBrakeTorque = front ? 0.0f : cHandBrakeTorque;
        wheel->mRadius = cWheelRadius;
        wheel->mWidth = cWheelWidth;
        wheel->mSuspensionMinLength = cSuspensionMinLength;
        wheel->mSuspensionMaxLength = cSuspensionMaxLength;
        wheel->mSuspensionSpring.mFrequency = cSuspensionFrequency;
        wheel->mSuspensionSpring.mDamping = cSuspensionDamping;

        const float lateral_grip = front ? cFrontLateralGrip : cRearLateralGrip;
        wheel->mLateralFriction.Clear();
        wheel->mLateralFriction.AddPoint(0.0f, 0.0f);
        wheel->mLateralFriction.AddPoint(3.0f, lateral_grip);
        wheel->mLateralFriction.AddPoint(20.0f, lateral_grip * 0.82f);
        wheel->mLongitudinalFriction.Clear();
        wheel->mLongitudinalFriction.AddPoint(0.0f, 0.0f);
        wheel->mLongitudinalFriction.AddPoint(0.06f, cLongitudinalGrip);
        wheel->mLongitudinalFriction.AddPoint(0.2f, cLongitudinalGrip * 0.83f);

        vehicle.mWheels.push_back(wheel);
    }

    JPH::WheeledVehicleControllerSettings* controller = new JPH::WheeledVehicleControllerSettings;
    controller->mEngine.mMaxTorque = cEngineTorque;
    controller->mEngine.mMinRPM = cEngineIdleRPM;
    controller->mEngine.mMaxRPM = cEngineMaxRPM;
    controller->mTransmission.mShiftUpRPM = cShiftUpRPM;
    controller->mTransmission.mShiftDownRPM = cShiftDownRPM;
    controller->mTransmission.mSwitchTime = 0.25f;
    controller->mTransmission.mClutchReleaseTime = 0.15f;
    controller->mTransmission.mSwitchLatency = 0.4f;

    controller->mDifferentials.resize(2);
    controller->mDifferentials[0].mLeftWheel = WHEEL_FRONT_LEFT;
    controller->mDifferentials[0].mRightWheel = WHEEL_FRONT_RIGHT;
    controller->mDifferentials[0].mEngineTorqueRatio = 0.4f;
    controller->mDifferentials[1].mLeftWheel = WHEEL_REAR_LEFT;
    controller->mDifferentials[1].mRightWheel = WHEEL_REAR_RIGHT;
    controller->mDifferentials[1].mEngineTorqueRatio = 0.6f;

    vehicle.mController = controller;

    vehicle.mAntiRollBars.resize(2);
    vehicle.mAntiRollBars[0].mLeftWheel = WHEEL_FRONT_LEFT;
    vehicle.mAntiRollBars[0].mRightWheel = WHEEL_FRONT_RIGHT;
    vehicle.mAntiRollBars[0].mStiffness = cAntiRollStiffness;
    vehicle.mAntiRollBars[1].mLeftWheel = WHEEL_REAR_LEFT;
    vehicle.mAntiRollBars[1].mRightWheel = WHEEL_REAR_RIGHT;
    vehicle.mAntiRollBars[1].mStiffness = cAntiRollStiffness;

    m_Vehicle = new JPH::VehicleConstraint(*body, vehicle);
    m_WheelFilter.mCarBody = m_CarBody;
    m_WheelFilter.mGodMode = m_GodMode;

    JPH::VehicleCollisionTester* tester = new JPH::VehicleCollisionTesterCastCylinder(EPhysicsObjectLayers::MOVING, 0.05f);
    tester->SetBodyFilter(&m_WheelFilter);
    m_Vehicle->SetVehicleCollisionTester(tester);

    system->AddConstraint(m_Vehicle);
    system->AddStepListener(m_Vehicle);
}


void CoastRallyScript::DestroyCar()
{
    if (m_Vehicle != nullptr)
    {
        GetPhysicsSystem()->RemoveStepListener(m_Vehicle);
        GetPhysicsSystem()->RemoveConstraint(m_Vehicle);
        m_Vehicle = nullptr;
    }

    if (!m_CarBody.IsInvalid())
    {
        JPH::BodyInterface& bodies = GetBodyInterface();
        bodies.RemoveBody(m_CarBody);
        bodies.DestroyBody(m_CarBody);
        m_CarBody = JPH::BodyID();
    }
}


JPH::WheeledVehicleController* CoastRallyScript::GetController() const
{
    return m_Vehicle != nullptr ? static_cast<JPH::WheeledVehicleController*>(m_Vehicle->GetController()) : nullptr;
}


void CoastRallyScript::PlaceCar(const Vec3& inPosition, const Vec3& inForward)
{
    if (m_CarBody.IsInvalid())
        return;

    const Vec3 forward = glm::normalize(Vec3(inForward.x, 0.0f, inForward.z) + Vec3(0.0f, 0.0f, 1e-4f));
    const Quat rotation = YawRotation(GetYaw(forward));

    JPH::BodyInterface& bodies = GetBodyInterface();
    bodies.SetPositionAndRotation(m_CarBody, ToJolt(inPosition + Vec3(0.0f, cSpawnHeight, 0.0f)), ToJolt(rotation), JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(m_CarBody, JPH::Vec3::sZero(), JPH::Vec3::sZero());

    for (int index = 0; index < WHEEL_COUNT; index++)
        m_Vehicle->GetWheel(index)->SetAngularVelocity(0.0f);

    if (JPH::WheeledVehicleController* controller = GetController())
    {
        controller->GetEngine().SetCurrentRPM(cEngineIdleRPM);
        controller->GetTransmission().Set(1, 1.0f);
        controller->SetDriverInput(0.0f, 0.0f, 0.0f, 0.0f);
    }

    m_CarPosition = inPosition + Vec3(0.0f, cSpawnHeight, 0.0f);
    m_CarRotation = rotation;
    m_CarVelocity = Vec3(0.0f);
    m_PreviousVelocity = Vec3(0.0f);
    m_Steer = 0.0f;
    m_PreviousForward = 1.0f;
    m_FlipTimer = 0.0f;
    m_AirTime = 0.0f;
    m_CameraYaw = GetYaw(forward);
    m_CameraPosition = m_CarPosition - forward * cChaseCameras[0].mDistance + Vec3(0.0f, cChaseCameras[0].mHeight, 0.0f);
}


int CoastRallyScript::FindNearestWaypoint(const Vec3& inPosition) const
{
    int nearest = -1;
    float nearest_distance = FLT_MAX;

    for (int index = 0; index < int(m_Waypoints.size()); index++)
    {
        const float distance = glm::distance(m_Waypoints[index].mPosition, inPosition);

        if (distance < nearest_distance)
        {
            nearest = index;
            nearest_distance = distance;
        }
    }

    return nearest;
}


void CoastRallyScript::ResetCar()
{
    const int nearest = FindNearestWaypoint(m_CarPosition);

    if (nearest < 0)
        PlaceCar(m_Spawn.mPosition, m_Spawn.mRotation * Vec3(0.0f, 0.0f, 1.0f));
    else
        PlaceCar(m_Waypoints[nearest].mPosition, m_Waypoints[nearest].mForward);

    PlaySfx(cResetSound, 0.6f);
}


float CoastRallyScript::GetForwardSpeed() const
{
    return glm::dot(m_CarVelocity, m_CarRotation * Vec3(0.0f, 0.0f, 1.0f));
}


int CoastRallyScript::GetGroundedWheels() const
{
    int count = 0;

    for (int index = 0; m_Vehicle != nullptr && index < WHEEL_COUNT; index++)
        count += m_Vehicle->GetWheel(index)->HasContact() ? 1 : 0;

    return count;
}


float CoastRallyScript::GetSkidAmount() const
{
    if (m_Vehicle == nullptr || GetGroundedWheels() < 2)
        return 0.0f;

    const Vec3 right = m_CarRotation * Vec3(1.0f, 0.0f, 0.0f);
    const float lateral = glm::abs(glm::dot(m_CarVelocity, right));
    float skid = glm::clamp(( lateral - 2.5f ) / 7.0f, 0.0f, 1.0f);

    for (int index = 0; index < WHEEL_COUNT; index++)
    {
        const JPH::WheelWV* wheel = static_cast<const JPH::WheelWV*>(m_Vehicle->GetWheel(index));

        if (wheel->HasContact() && glm::length(m_CarVelocity) > 2.0f)
            skid = glm::max(skid, glm::clamp(( glm::abs(wheel->mLongitudinalSlip) - 0.25f ) * 1.5f, 0.0f, 1.0f));
    }

    return skid;
}


void CoastRallyScript::UpdateCar(float inDeltaTime)
{
    JPH::WheeledVehicleController* controller = GetController();

    if (controller == nullptr)
        return;

    ApplyGodMode();

    JPH::BodyInterface& bodies = GetBodyInterface();

    JPH::RVec3 position;
    JPH::Quat rotation;
    bodies.GetPositionAndRotation(m_CarBody, position, rotation);

    m_CarPosition = ToVec3(position);
    m_CarRotation = ToQuat(rotation);
    m_CarVelocity = ToVec3(bodies.GetLinearVelocity(m_CarBody));

    const Vec3 forward_axis = m_CarRotation * Vec3(0.0f, 0.0f, 1.0f);
    const Vec3 up_axis = m_CarRotation * Vec3(0.0f, 1.0f, 0.0f);
    const Vec3 right_axis = m_CarRotation * Vec3(-1.0f, 0.0f, 0.0f);
    const float speed = GetForwardSpeed();
    const int grounded = GetGroundedWheels();

    const bool key_forward = m_Input->IsKeyDown(Key::W) || m_Input->IsKeyDown(Key::UP);
    const bool key_back = m_Input->IsKeyDown(Key::S) || m_Input->IsKeyDown(Key::DOWN);
    const bool key_left = m_Input->IsKeyDown(Key::A) || m_Input->IsKeyDown(Key::LEFT);
    const bool key_right = m_Input->IsKeyDown(Key::D) || m_Input->IsKeyDown(Key::RIGHT);

    float forward = key_forward ? 1.0f : ( key_back ? -1.0f : 0.0f );
    float target_steer = ( key_right ? 1.0f : 0.0f ) - ( key_left ? 1.0f : 0.0f );
    float brake = 0.0f;
    const float hand_brake = m_Input->IsKeyDown(Key::SPACE) ? 1.0f : 0.0f;

    m_Boosting = m_Input->IsKeyDown(Key::LSHIFT) && key_forward;

    if (*m_AutoDrive && !m_Waypoints.empty())
    {
        const int nearest = FindNearestWaypoint(m_CarPosition);
        const Vec3 target = m_Waypoints[( nearest + 3 ) % m_Waypoints.size()].mPosition;
        const Vec3 local = glm::inverse(m_CarRotation) * ( target - m_CarPosition );

        target_steer = glm::clamp(-glm::atan(local.x, local.z) * 2.5f, -1.0f, 1.0f);
        forward = speed < 18.0f ? 1.0f : 0.0f;
    }

    const float steer_rate =glm::abs(target_steer) > glm::abs(m_Steer) && target_steer * m_Steer >= 0.0f ? cSteerSpeed : cSteerReturnSpeed;
    m_Steer += glm::clamp(target_steer - m_Steer, -steer_rate * inDeltaTime, steer_rate * inDeltaTime);

    const float steer_scale = glm::mix(1.0f, cHighSpeedSteerScale, glm::clamp(glm::abs(speed) / cSteerFalloffSpeed, 0.0f, 1.0f));

    if (m_PreviousForward * forward < 0.0f)
    {
        if (( forward > 0.0f && speed < -0.1f ) || ( forward < 0.0f && speed > 0.1f ))
        {
            forward = 0.0f;
            brake = 1.0f;
        }
        else
        {
            m_PreviousForward = forward;
        }
    }

    m_Throttle = glm::abs(forward);
    m_Braking = brake > 0.0f || hand_brake > 0.0f;

    controller->SetDriverInput(forward, m_Steer * steer_scale, brake, hand_brake);

    if (forward != 0.0f || m_Steer != 0.0f || brake != 0.0f || hand_brake != 0.0f)
        bodies.ActivateBody(m_CarBody);

    Vec3 velocity = m_CarVelocity;

    if (grounded > 0)
    {
        velocity -= up_axis * cDownforce * speed * speed * inDeltaTime;

        if (m_Boosting && grounded >= 2 && speed < cMaxBoostSpeed)
            velocity += forward_axis * cBoostAcceleration * inDeltaTime;
    }

    if (velocity != m_CarVelocity)
        bodies.SetLinearVelocity(m_CarBody, ToJolt(velocity));

    m_DriftTimer = hand_brake > 0.0f ? cDriftAssistDelay : glm::max(0.0f, m_DriftTimer - inDeltaTime);

    if (grounded >= 3 && glm::abs(speed) > 3.0f && m_DriftTimer <= 0.0f)
    {
        const float steer_angle = m_Steer * steer_scale * cMaxSteerAngle;
        const float grip_limit = cYawAssistGrip / glm::abs(speed);
        const float target_yaw = glm::clamp(-speed * glm::tan(steer_angle) / cWheelBase, -grip_limit, grip_limit);

        const Vec3 angular = ToVec3(bodies.GetAngularVelocity(m_CarBody));
        const float yaw = glm::dot(angular, up_axis);
        const float gain = m_Steer != 0.0f ? cYawAssist : cYawAssistStraight;

        bodies.SetAngularVelocity(m_CarBody, ToJolt(angular + up_axis * ( target_yaw - yaw ) * glm::min(gain * inDeltaTime, 1.0f)));
    }

    if (grounded > 0 && grounded < 4 && up_axis.y < cSelfRightingTilt)
    {
        const Vec3 angular = ToVec3(bodies.GetAngularVelocity(m_CarBody));
        bodies.SetAngularVelocity(m_CarBody, ToJolt(angular + glm::cross(up_axis, Vec3(0.0f, 1.0f, 0.0f)) * cSelfRighting * inDeltaTime));
    }

    if (grounded == 0)
    {
        const float pitch = ( key_forward ? 1.0f : 0.0f ) - ( key_back ? 1.0f : 0.0f );
        const Vec3 spin = up_axis * -target_steer * cAirControl + right_axis * -pitch * cAirControl * 0.6f;
        const Vec3 angular = ToVec3(bodies.GetAngularVelocity(m_CarBody));

        bodies.SetAngularVelocity(m_CarBody, ToJolt(angular + spin * inDeltaTime));

        m_AirTime += inDeltaTime;
    }
    else
    {
        if (m_AirTime > 0.35f && m_PreviousVelocity.y < -cLandSpeed)
        {
            PlaySfx(cLandSound, glm::clamp(-m_PreviousVelocity.y / 14.0f, 0.3f, 1.0f), std::uniform_real_distribution<float>(0.9f, 1.05f)(m_Random));
            m_Shake = glm::max(m_Shake, glm::clamp(-m_PreviousVelocity.y / 25.0f, 0.1f, 0.6f));
        }

        if (m_AirTime > 1.0f)
        {
            AddPopup(std::format("BIG AIR  {:.1f}S", m_AirTime), cSun);
            m_BestAirTime = glm::max(m_BestAirTime, m_AirTime);
        }

        m_AirTime = 0.0f;
    }

    const Vec3 velocity_change = m_CarVelocity - m_PreviousVelocity;
    const float impact = glm::length(Vec2(velocity_change.x, velocity_change.z));

    if (impact > cImpactSpeed && m_Time - m_LastImpactTime > 0.25f)
    {
        PlaySfx(cImpactSound, glm::clamp(impact / 14.0f, 0.25f, 1.0f), std::uniform_real_distribution<float>(0.9f, 1.1f)(m_Random));
        m_Shake = glm::max(m_Shake, glm::clamp(impact / 20.0f, 0.15f, 0.8f));
        m_LastImpactTime = m_Time;
    }

    m_PreviousVelocity = m_CarVelocity;
    m_Odometer += glm::length(Vec2(m_CarVelocity.x, m_CarVelocity.z)) * inDeltaTime;
    m_TopSpeed = glm::max(m_TopSpeed, glm::abs(speed));

    if (m_CarPosition.y < cDrownHeight)
    {
        PlaySfx(cSplashSound, 0.9f);
        AddPopup("SPLASH!", cSea);
        ResetCar();
        return;
    }

    if (up_axis.y < cFlippedUp && glm::length(m_CarVelocity) < 3.0f)
        m_FlipTimer += inDeltaTime;
    else
        m_FlipTimer = 0.0f;

    if (m_FlipTimer > cFlipResetTime)
    {
        PlaceCar(m_CarPosition - Vec3(0.0f, cSpawnHeight * 0.5f, 0.0f), forward_axis);
        PlaySfx(cResetSound, 0.6f);
    }
}


void CoastRallyScript::UpdateCarVisuals()
{
    if (m_Vehicle == nullptr)
        return;

    SetTransform(m_CarModel.mRoot, m_CarPosition, m_CarRotation);

    for (int index = 0; index < WHEEL_COUNT; index++)
    {
        const bool left = index == WHEEL_FRONT_LEFT || index == WHEEL_REAR_LEFT;
        const JPH::RMat44 transform = m_Vehicle->GetWheelWorldTransform(index, left ? -JPH::Vec3::sAxisX() : JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY());

        SetTransform(m_WheelNodes[index], ToVec3(transform.GetTranslation()), ToQuat(transform.GetQuaternion()));
    }

    const Quat headlight_rotation = m_CarRotation * YawRotation(glm::pi<float>()) * glm::angleAxis(-cHeadlightPitch, Vec3(1.0f, 0.0f, 0.0f));

    for (size_t index = 0; index < m_HeadlightEntities.size(); index++)
    {
        const Vec3 offset = cHeadlightOffset * Vec3(index == 0 ? 1.0f : -1.0f, 1.0f, 1.0f);
        SetTransform(m_HeadlightEntities[index], m_CarPosition + m_CarRotation * offset, headlight_rotation);
    }

    SetMaterial(m_BrakeNode, m_Braking ? m_BrakeMaterial : m_PaletteMaterial);
}

} // RK
